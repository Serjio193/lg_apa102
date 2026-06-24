Import("env")

from pathlib import Path
import subprocess
import shutil

project_dir = Path(env["PROJECT_DIR"])
ui_dir = project_dir / "ui"

def build_ui():
    node = shutil.which("node")
    if not node:
        raise RuntimeError("node not found in PATH")
    subprocess.run([node, "build.mjs"], cwd=ui_dir, check=True)

build_ui()
