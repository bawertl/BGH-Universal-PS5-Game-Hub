#!/usr/bin/env python3
"""Launch only the development preview on this PC. Does not contact a console."""
from pathlib import Path
import os
import sys
import webbrowser
import threading

PROJECT = Path(__file__).resolve().parents[1]
os.chdir(PROJECT)
sys.path.insert(0, str(PROJECT / 'src'))

if __name__ == '__main__':
    # Separate process uses 127.0.0.1 only, no external server/cloud.
    threading.Timer(1.0, lambda: webbrowser.open('http://127.0.0.1:8765/')).start()
    from host_server import main
    main()
