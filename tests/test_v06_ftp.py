"""Local loopback FTP protocol regression; never accesses a console or the network."""
from pathlib import Path
import socket
import subprocess
import threading

ROOT = Path(__file__).resolve().parents[1]
NATIVE = ROOT / 'native_ps5'


def test_recover_denied_posix_root_via_readonly_local_ftp(tmp_path):
    subprocess.run(['make', '-C', str(NATIVE), 'CXX=g++', 'ftp-test-cli'], check=True, capture_output=True)
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.bind(('127.0.0.1', 0))
    listener.listen(1)
    listener.settimeout(10)
    port = listener.getsockname()[1]
    errors = []
    commands = []
    done = threading.Event()

    def serve():
        try:
            for _connection in range(2):
                control, _ = listener.accept()
                handle_connection(control)
        except Exception as ex:
            errors.append(repr(ex))
        finally:
            listener.close()
            done.set()

    def handle_connection(control):
            with control:
                control.settimeout(7)
                f = control.makefile('rwb', buffering=0)
                def reply(line):
                    f.write((line + '\r\n').encode())
                reply('220 FTP test server')
                pasv = None
                while True:
                    line = f.readline()
                    if not line:
                        break
                    command = line.decode('ascii', 'replace').strip()
                    commands.append(command)
                    if command.startswith('USER '):
                        reply('230 User logged in')
                    elif command == 'TYPE I':
                        reply('200 Type set to I')
                    elif command.startswith('CWD '):
                        if command == 'CWD /does-not-exist':
                            reply('250 Directory changed')
                        else:
                            reply('550 Not found')
                    elif command == 'PASV':
                        pasv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                        pasv.bind(('127.0.0.1', 0))
                        pasv.listen(1)
                        pasv.settimeout(7)
                        p = pasv.getsockname()[1]
                        reply(f'227 Entering Passive Mode (127,0,0,1,{p // 256},{p % 256})')
                    elif command == 'MLSD':
                        reply('150 Opening data transfer')
                        data, _ = pasv.accept()
                        with data:
                            data.sendall(b'type=file;size=123;modify=20261014081000; Example.xci\r\n')
                        pasv.close()
                        pasv = None
                        reply('226 Transfer complete')
                    else:
                        reply('500 Unexpected command')
                if pasv:
                    pasv.close()

    thread = threading.Thread(target=serve, daemon=True)
    thread.start()
    run = subprocess.run([str(NATIVE / 'ftp-test-cli'), str(port), '/does-not-exist'],
                         capture_output=True, text=True, timeout=20)
    assert run.returncode == 0, (run.stdout, run.stderr, errors)
    assert done.wait(9), 'mock FTP service did not finish'
    assert not errors, errors
    assert 'Recovered game' in run.stdout
    assert all(not x.startswith(('STOR ', 'DELE ', 'RMD ', 'MKD ', 'SITE ')) for x in commands)
    assert 'MLSD' in commands
