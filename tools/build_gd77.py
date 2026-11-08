#!/usr/bin/env python3
"""Reconstructed GNU GD77 build; no flashing. Empty codec sections only.
Not the missing upstream MCUXpresso project. Logs include all compiler commands.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument("--cc", default="arm-none-eabi-gcc")
p.add_argument("--newlib", type=Path, help="optional extracted Debian newlib usr directory")
p.add_argument("--out", type=Path, required=True)
p.add_argument("--source", type=Path, default=ROOT)
p.add_argument("--experimental-mdc-rf",action="store_true",help="GD77 lab RF sender; GPL codec included, RF validation NOT RUN")
p.add_argument("--lto", action="store_true", help="use whole-program optimization")
p.add_argument("--link", action="store_true", help="link using EMPTY codec sections; never a flashable donor image")
a = p.parse_args()
source = a.source.resolve()
out = a.out.resolve(); out.mkdir(parents=True, exist_ok=True)
cc = shutil.which(a.cc)
if not cc: sys.exit("BLOCKED: arm-none-eabi-gcc missing")
firmware = source/"firmware"
for name in ("linkerscripts",):
    dest=out/name
    if not dest.exists(): dest.symlink_to(firmware/name, target_is_directory=True)
work=out/"build"; work.mkdir(exist_ok=True)
inc=["-I"+str(firmware)] + ["-I"+str(d) for d in sorted(firmware.rglob("*")) if d.is_dir()]
flags=["-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard",
       "-Os", "-g3", "-std=gnu11", "-ffunction-sections", "-fdata-sections", "-fno-common",
       "-DPLATFORM_GD77", "-DHAS_GPS=1", "-DCPU_MK22FN512VLL12", "-DSDK_OS_FREE_RTOS", "-D__USE_CMSIS",
       '-DGITVERSION="migration-lab"', "-Werror=implicit-function-declaration", *inc]
if a.experimental_mdc_rf:
    flags += ["-DGD77_EXPERIMENTAL_MDC_RF=1",
              "-I"+str(source/"tools/mdc1200")]
if a.lto: flags += ["-flto"]
if a.newlib: flags += ["-isystem", str(a.newlib.resolve()/"include/newlib")]
files=sorted(firmware.rglob("*.c"))
# Segger syscalls are optional debugger/Redlib integration, not newlib runtime.
files=[f for f in files if "/SeggerRTT/" not in str(f) and "/include/" not in str(f)
       and not (f.name.startswith("heap_") and f.name != "heap_4.c")]
if a.experimental_mdc_rf:files.append(source/"tools/mdc1200/mdc1200_codec.c")
commands=[]
def compile_one(f):
    relative=f.relative_to(firmware) if f.is_relative_to(firmware) else f.relative_to(source)
    obj=work/(str(relative).replace("/","_")+".o")
    cmd=[cc,*flags,"-c",str(f),"-o",str(obj)]
    result=subprocess.run(cmd,cwd=work,capture_output=True,text=True)
    return f,obj,cmd,result
with ThreadPoolExecutor(max_workers=3) as pool:
    results=list(pool.map(compile_one,files))
failed=[];objects=[]
with (out/"compile.log").open("w") as log:
    log.write(subprocess.check_output([cc,"--version"],text=True))
    for f,obj,cmd,r in results:
        commands.append(cmd)
        log.write("\n"+str(f.relative_to(source))+"\n"+r.stdout+r.stderr)
        if r.returncode:failed.append(str(f.relative_to(source)))
        else:objects.append(obj)
(out/"commands.json").write_text(json.dumps(dict(common=commands[0][:-4],
    jobs=[cmd[-4:] for cmd in commands]),indent=2)+"\n")
print(f"ARM objects: {len(objects)} passed, {len(failed)} failed; log {out/'compile.log'}",flush=True)
if failed:
    print("Failed:",*failed,sep="\n")
    sys.exit(1)
if not a.link:sys.exit(0)
# Copy existing cleaner to temporary output to avoid changing repository modes/data.
cleaner=out/"codec_cleaner.Linux"
shutil.copyfile(firmware/"tools/codec_cleaner.Linux",cleaner)
cleaner.chmod(0o700)
codec=out/"linkerdata";codec.mkdir(exist_ok=True)
r=subprocess.run([str(cleaner),"-C"],cwd=codec,capture_output=True,text=True)
(out/"codec-placeholder.log").write_text(r.stdout+r.stderr)
if r.returncode:sys.exit("BLOCKED: empty codec generation failed")
asm=work/"codec_bin.o"
subprocess.run([cc,*flags,"-c",str(firmware/"source/dmr_codec/codec_bin.S"),"-o",str(asm)],cwd=work,check=True)
objects.append(asm)
elf=out/"GD77-empty-codec.elf"
cmd=[cc,*flags[:4],"-nostartfiles","-Wl,--gc-sections", "-Wl,-Map="+str(out/"GD77.map"),
     "-T",str(firmware/"linkerscripts/firmware_newlib.ld"), *map(str,objects)]
if a.newlib:cmd += ["-L"+str(a.newlib.resolve()/"lib/arm-none-eabi/newlib/thumb/v7e-m+fp/hard")]
if a.lto: cmd += ["-flto", "-Os"]
cmd += ["-o",str(elf)]
r=subprocess.run(cmd,cwd=work,capture_output=True,text=True)
(out/"link-command.json").write_text(json.dumps(cmd,indent=2)+"\n")
(out/"link.log").write_text(r.stdout+r.stderr)
if r.returncode:
    elf.unlink(missing_ok=True)
    sys.exit("BLOCKED: link failed; see link.log")
size=str(Path(cc).with_name("arm-none-eabi-size"))
summary=subprocess.check_output([size,str(elf)],text=True)
(out/"size.log").write_text(summary)
print(summary)
print("Linked ELF contains EMPTY codec placeholders; no flash action or donor binary.")
