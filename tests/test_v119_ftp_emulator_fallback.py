"""V1.1.9 hotfix: PS5 app sandbox may deny local stat but local FTP can see the app."""
from pathlib import Path
import json
import socket
import socketserver
import subprocess
import threading

ROOT = Path(__file__).resolve().parents[1]


def test_rpcs3_param_schema_and_sandbox_ftp_fallback(tmp_path):
    # Exact structural shape from KB-RPCS3's actual PS5 param.json.
    meta = json.dumps({
        'titleId': 'PPSA99303',
        'localizedParameters': {
            'defaultLanguage': 'en-US', 'en-US': {'titleName': 'KB-RPCS3'}},
        'gameIntent': {'permittedIntents': [{'intentType': 'launchActivity'}]},
    }).encode()
    files = {'eboot.bin', 'rpcs3ps5-launch.elf', 'rpcs3ps5-io.elf'}
    contents = b'\r\n'.join(f'type=file; {name}'.encode() for name in sorted(files)) + b'\r\n' + b'type=dir; sce_sys\r\n'

    class Control(socketserver.BaseRequestHandler):
        def handle(self):
            self.request.settimeout(6)
            self.request.sendall(b'220 fake read-only PS5 FTP\r\n')
            data_listener = None
            stream = self.request.makefile('rb')
            try:
                while True:
                    raw = stream.readline()
                    if not raw:
                        break
                    cmd = raw.decode('ascii', 'replace').strip()
                    verb = cmd.split(' ', 1)[0]
                    if verb == 'USER':
                        self.request.sendall(b'230 ok\r\n')
                    elif verb == 'TYPE':
                        self.request.sendall(b'200 ok\r\n')
                    elif verb == 'CWD':
                        self.request.sendall(b'250 path exists\r\n' if cmd[4:] == '/data/homebrew/PPSA99303' else b'550 missing\r\n')
                    elif verb == 'PASV':
                        data_listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                        data_listener.bind(('127.0.0.1', 0))
                        data_listener.listen(1)
                        port = data_listener.getsockname()[1]
                        self.request.sendall(f'227 entering passive mode (127,0,0,1,{port//256},{port%256})\r\n'.encode())
                    elif verb in ('MLSD', 'RETR') and data_listener is not None:
                        self.request.sendall(b'150 opening data\r\n')
                        dataconn, _ = data_listener.accept()
                        with dataconn:
                            dataconn.sendall(contents if verb == 'MLSD' else meta)
                        data_listener.close()
                        data_listener = None
                        self.request.sendall(b'226 complete\r\n')
                    else:
                        self.request.sendall(b'500 unsupported\r\n')
            finally:
                stream.close()
                if data_listener is not None:
                    data_listener.close()

    class Server(socketserver.ThreadingTCPServer):
        allow_reuse_address = True
        daemon_threads = True

    code = r'''
#include "emulator_detection.hpp"
#include "ftp_readonly.hpp"
#include <cassert>
#include <string>
int main(int argc, char **argv) {
    assert(argc == 2);
    const auto profiles = ulg::emulator_detection::built_in_profiles();
    const std::string path = "/data/homebrew/PPSA99303";
    const auto local = ulg::emulator_detection::detect(path, "PS3", profiles);
    assert(local.status == ulg::emulator_detection::Detection::Status::invalid_path);
    ulg::FtpReadonly ftp(std::stoi(argv[1]));
    const auto detected = ulg::emulator_detection::detect_via_ftp(path, "PS3", profiles, ftp);
    assert(detected.status == ulg::emulator_detection::Detection::Status::known_emulator);
    assert(detected.title_id == "PPSA99303" && detected.profile_id == "rpcs3ps5");
    assert(detected.name == "KB-RPCS3" && detected.via_ftp);
    assert(ulg::emulator_detection::description(detected).find("(FTP) erkannt") != std::string::npos);
    const auto other = ulg::emulator_detection::detect_via_ftp(path, "Wii", profiles, ftp);
    assert(other.status == ulg::emulator_detection::Detection::Status::unidentified_app);
}
'''
    cpp = tmp_path / 'probe_ftp.cpp'
    cpp.write_text(code)
    exe = tmp_path / 'probe_ftp'
    subprocess.run(['g++', '-std=c++20', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-I', str(ROOT / 'native_ps5'), str(cpp),
                    str(ROOT / 'native_ps5/ftp_readonly.cpp'), '-o', str(exe)],
                   check=True, capture_output=True)
    with Server(('127.0.0.1', 0), Control) as server:
        t = threading.Thread(target=server.serve_forever, daemon=True)
        t.start()
        try:
            subprocess.run([str(exe), str(server.server_address[1])], check=True,
                           capture_output=True, timeout=20)
        finally:
            server.shutdown()
            t.join(timeout=3)


def test_ftp_detection_wired_and_no_launch_changes():
    cpp = (ROOT / 'native_ps5/universal_launcher.cpp').read_text()
    detector = (ROOT / 'native_ps5/emulator_detection.hpp').read_text()
    assert 'detect_via_ftp(' in cpp
    assert 'ulg::FtpReadonly ftp;' in cpp
    assert 'ftp.list(path,200)' in detector
    assert 'ftp.retrieve(path+"/sce_sys/param.json",bytes,65536)' in detector
    assert 'localized.get("en-US")' in detector
    assert 'plan.target=d.title_id;plan.supported=true' in cpp
