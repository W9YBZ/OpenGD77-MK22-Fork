#!/usr/bin/env python3
"""Reproducible host tests; deliberately never builds/flashes a radio image."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]
incs=["-I"+str(d) for d in (ROOT/"firmware").rglob("*") if d.is_dir()]
with tempfile.TemporaryDirectory() as td:
    for test in ("settings","tail","tx_helpers","radio_cache","mdc_schedule","mdc_rf","eeprom_bus","tx_helpers_rf"):
        exe=str(Path(td)/test)
        source_test="tx_helpers" if test=="tx_helpers_rf" else test
        defines=["-DGD77_EXPERIMENTAL_MDC_RF=1"] if test=="tx_helpers_rf" else []
        extra=[str(ROOT/"tools/mdc1200/mdc1200_codec.c")] if test=="mdc_rf" else []
        cmd=["gcc","-std=gnu11","-Wall","-Wextra","-Wno-int-to-pointer-cast",
             "-Wno-pointer-to-int-cast","-ffunction-sections","-fdata-sections",
             "-fsanitize=undefined","-fno-sanitize-recover=all",
             "-DSDK_OS_FREE_RTOS","-DPLATFORM_GD77","-DHAS_GPS=1","-DCPU_MK22FN512VLL12",
             *incs,*defines,"-I"+str(ROOT/"tools/mdc1200"),str(ROOT/f"tests/feature_migration/test_{source_test}.c"),*extra,"-Wl,--gc-sections","-o",exe]
        subprocess.run(cmd,check=True)
        subprocess.run([exe],check=True)
subprocess.run(["python3",str(ROOT/"tests/feature_migration/check_syntax.py")],check=True)
subprocess.run(["python3",str(ROOT/"tests/feature_migration/test_mdc.py")],check=True)
