import subprocess
Import("env")

try:
    tag = subprocess.run(
        ["git", "describe", "--tags", "--always"],
        capture_output=True, text=True, check=True
    ).stdout.strip()
except Exception:
    tag = ""

if not tag:
    tag = "dev"

env.Append(CPPDEFINES=[("FIRMWARE_VERSION", f'\\"{ tag }\\"')])
