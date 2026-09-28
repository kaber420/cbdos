Import("env")
import subprocess
import os
import sys

project_dir = env.get("PROJECT_DIR")
workspace_root = os.path.abspath(os.path.join(project_dir, "..", ".."))

cdtc_script = os.path.join(workspace_root, "tools", "cdtc.py")
input_json = os.path.join(workspace_root, "boards", "jc3248w535.json")
output_header = os.path.join(project_dir, "include", "cbdos_device_tree.h")

print(f"[CDT-S3] Generando Device Tree desde {input_json}...")
res = subprocess.run([sys.executable, cdtc_script, input_json, output_header], capture_output=True, text=True)
if res.returncode != 0:
    print(f"[CDT-S3 Error] Fallo al compilar Device Tree:\n{res.stderr}", file=sys.stderr)
    env.Exit(1)
else:
    print(f"[CDT-S3] {res.stdout.strip()}")
