import subprocess
import sys
import os

path = os.path.dirname(os.path.abspath(__file__))

if sys.platform == "win32":
    subprocess.run(["cmd", "/c", "start", "cmd", "/k", f"cd /d {path} & opencode"], shell=True)
else:
    os.chdir(path)
    subprocess.run(["opencode"])
