import subprocess
import os
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

include_dir = env.subst("$PROJECT_INCLUDE_DIR")
with open(os.path.join(include_dir, "version.h"), "w") as f:
    f.write('#pragma once\n')
    f.write(f'#define FIRMWARE_VERSION "{tag}"\n')

insights_key = os.environ.get("ESP_INSIGHT_AUTH_KEY", "")
if insights_key:
    env.Append(CPPDEFINES=[("ESP_INSIGHT_AUTH_KEY", '\\"' + insights_key + '\\"')])
