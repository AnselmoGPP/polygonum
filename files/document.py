#!/usr/bin/env python3

import os
import shutil
import subprocess
from pathlib import Path

def run_command(command):
    result = subprocess.run(command, shell=True)
    if result.returncode != 0:
        print(f"Error: Command failed -> {command}")
        exit(1)

def main():
    if not os.path.exists("../_BUILD"):
        os.makedirs("../_BUILD")
        
    if not os.path.exists("../_BUILD/docs"):
        os.makedirs("../_BUILD/docs")
    
    os.chdir("../projects")
    projectsDir = Path.cwd()
    
    for folder in projectsDir.iterdir():
        if folder.is_dir():
            os.chdir(folder.name)
            run_command("doxygen Doxyfile")
            os.rename("html", folder.name)
            shutil.move(folder.name, "../../_BUILD/docs")
            print(f"> Docs generated: {folder.name}")
            os.chdir("..")

    print("\n>> All docs generated")

# Ensure main only runs when script is executed directly, not when imported as module.
if __name__ == "__main__":
    main()


# Other actions:
#   Generate template config file: doxygen -g Doxyfile
#   Update config file: doxygen -u Doxyfile
