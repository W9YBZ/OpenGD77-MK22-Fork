#!/usr/bin/env python3
"""Host syntax only using real GD77 headers. This is NOT an ARM build."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
files = ["functions/settings.c", "functions/sound.c", "user_interface/menuSoundOptions.c",
         "user_interface/uiTxScreen.c", "hardware/AT1846S.c", "hardware/EEPROM.c", "functions/mdc1200RF.c"]
inc = ["-I" + str(p) for p in (root / "firmware").rglob("*") if p.is_dir()]
for platform in ("PLATFORM_GD77", "PLATFORM_GD77S", "PLATFORM_DM1801", "PLATFORM_RD5R"):
    for name in files:
        subprocess.run(["gcc", "-std=gnu11", "-fsyntax-only", "-Werror=implicit-function-declaration",
                        "-Wno-int-to-pointer-cast", "-Wno-pointer-to-int-cast",
                        "-DSDK_OS_FREE_RTOS", "-DCPU_MK22FN512VLL12",
                        "-D" + platform, *inc, str(root / "firmware/source" / name)], check=True)
    print("PASS syntax:", platform)
