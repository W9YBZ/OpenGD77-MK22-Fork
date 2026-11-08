#!/usr/bin/env python3
"""Reproducible host tests; deliberately never builds/flashes a radio image."""
from pathlib import Path
import argparse
import subprocess
import tempfile
parser=argparse.ArgumentParser()
parser.add_argument("--optimized",action="store_true",help="host -O2/-flto fault tests")
args=parser.parse_args()
ROOT=Path(__file__).resolve().parents[2]
incs=["-I"+str(d) for d in (ROOT/"firmware").rglob("*") if d.is_dir()]
with tempfile.TemporaryDirectory() as td:
    main_source=(ROOT/"firmware/source/main.c").read_text()
    start=main_source.index("static void keyBeepHandler(")
    end=main_source.index("static bool rxBeepsMustPlayMelody",start)
    (Path(td)/"key_beep_handler.inc").write_text(main_source[start:end])
    for test in ("settings","tail","tx_helpers","radio_cache","mdc_schedule","mdc_rf","eeprom_bus","tx_helpers_rf","newlib_heap","spi_flash","sound"):
        exe=str(Path(td)/test)
        source_test="tx_helpers" if test=="tx_helpers_rf" else test
        defines=["-DGD77_EXPERIMENTAL_MDC_RF=1"] if test=="tx_helpers_rf" else []
        extra=[str(ROOT/"tools/mdc1200/mdc1200_codec.c")] if test=="mdc_rf" else []
        if test=="newlib_heap":
            extra=[str(ROOT/"firmware/source/runtime/newlib_heap.c")]
            defines.append("-DGD77_NEWLIB_BUILD=1")
        cmd=["gcc","-std=gnu11","-Wall","-Wextra","-Wno-int-to-pointer-cast",
             "-Wno-pointer-to-int-cast","-ffunction-sections","-fdata-sections",
             "-fsanitize=undefined","-fno-sanitize-recover=all",
             "-DSDK_OS_FREE_RTOS","-DPLATFORM_GD77","-DHAS_GPS=1","-DCPU_MK22FN512VLL12",
             *incs,*defines,*( ["-O2","-flto"] if args.optimized else []),"-I"+td,"-I"+str(ROOT/"tools/mdc1200"),str(ROOT/f"tests/feature_migration/test_{source_test}.c"),*extra,"-Wl,--gc-sections","-o",exe]
        subprocess.run(cmd,check=True)
        subprocess.run([exe],check=True)
subprocess.run(["python3",str(ROOT/"tests/feature_migration/check_syntax.py")],check=True)
subprocess.run(["python3",str(ROOT/"tests/feature_migration/test_mdc.py")],check=True)
