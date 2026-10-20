"""V0.6 read-only directory diagnostics and DualSense folder management backend."""
from pathlib import Path
import json
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[1]
NATIVE=ROOT/'native_ps5'

def test_root_manager_and_browser(tmp_path):
    subprocess.run(['make','-C',str(NATIVE),'CXX=g++','roots-test-cli'],check=True,capture_output=True)
    directory=tmp_path/'browse';directory.mkdir()
    (directory/'first').mkdir();(directory/'second').mkdir()
    (directory/'external').symlink_to(tmp_path, target_is_directory=True)
    packaged=tmp_path/'paths.conf';packaged.write_text('PS2|'+str(directory)+'\n')
    save=tmp_path/'user.conf'
    r=subprocess.run([str(NATIVE/'roots-test-cli'),str(packaged),str(save),str(directory)],capture_output=True,text=True)
    assert r.returncode==0,r.stderr
    assert '0|Switch|' in save.read_text()

def test_diagnostic_errno_and_error_count(tmp_path):
    scanner=NATIVE/'scanner-host'
    subprocess.run(['make','-C',str(NATIVE),'CXX=g++','scanner-host'],check=True,capture_output=True)
    exists=tmp_path/'games';exists.mkdir();(exists/'Game.iso').write_bytes(b'abc')
    missing=tmp_path/'missing'
    file=tmp_path/'not_a_directory';file.write_bytes(b'abc')
    config=tmp_path/'paths.conf';config.write_text(
        f'PS2|{exists}\nPS2|{missing}\nPS2|{file}\nPS2|/mnt/usb98765/PS5SX2/games\n'
    )
    out=tmp_path/'output.json'
    subprocess.run([str(scanner),'--paths',str(config),'--output',str(out)],check=True,capture_output=True)
    obj=json.loads(out.read_text())
    assert len(obj['games'])==1
    assert obj['failed_operations']==3
    assert obj['failed_roots']==3
    assert len(obj['errors'])==3
    by_path={v['path']:v for v in obj['unavailable_paths']}
    assert by_path[str(missing)]['errno']==2
    assert by_path[str(missing)]['operation']=='lstat'
    assert by_path[str(file)]['errno']==20
    assert by_path[str(file)]['status']=='kein_ordner'
    assert by_path['/mnt/usb98765/PS5SX2/games']['status']=='usb_offline'

def test_add_remove_toggle_persistence_and_no_symlink(tmp_path):
    source=textwrap.dedent(r'''
    #include "root_manager.hpp"
    #include <fstream>
    #include <algorithm>
    #include <iostream>
    int main(int, char**argv){
      const std::string base=argv[1], config=base+"/paths.user.conf", defaults=base+"/packaged.conf";
      std::ofstream(defaults)<<"PS2|/data/PCSX2/games\n";
      std::vector<ulg::Root> roots{{"PS2",base+"/library",true},{"Switch",base+"/offline",false}};
      if(!ulg::save_user_roots(roots,config))return 1;
      const auto read=ulg::configured_roots(config,defaults);
      if(read.size()!=2 || !read[0].enabled || read[1].enabled)return 2;
      const auto browsed=ulg::browse_folders(base+"/library");
      if(browsed.entries.size()!=1 || browsed.entries[0].name!="valid")return 3;
      if(ulg::valid_root({"PS2","/data/../etc"}))return 4;
      if(ulg::valid_root({"Switch","/"}))return 5;
      if(ulg::valid_root({"Switch","/etc\nBAD"}))return 6;
      if(ulg::save_user_roots({{"PS2","/tmp"},{"PS2","/tmp"}},config))return 7;
      return 0;
    }
    ''')
    tmp=(tmp_path/'harness.cpp');tmp.write_text(source)
    test=tmp_path/'harness'
    subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror','-I',str(NATIVE),str(tmp),
                    str(NATIVE/'library_core.cpp'),str(NATIVE/'root_manager.cpp'),str(NATIVE/'ftp_readonly.cpp'),'-o',str(test)],check=True)
    lib=tmp_path/'library';lib.mkdir();(lib/'valid').mkdir()
    (lib/'link').symlink_to(tmp_path, target_is_directory=True)
    subprocess.run([str(test),str(tmp_path)],check=True)

def test_new_gui_syntax_and_kit_touch_forwarding():
    subprocess.run(['bash',str(ROOT/'tooling'/'test_native_gui_syntax.sh')],check=True)
    overlay=(ROOT/'tooling/install_ui_kit_overlay.py').read_text()
    assert 'count > 1 && input.is_pressed(Action::touch)' in overlay
    assert 'count == 1 ? 0u : kShellActions' in overlay
