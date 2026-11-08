#!/usr/bin/env python3
"""GNU GD77 release build. Checked ELF/BIN for CPS donor merge; never flashes."""
import argparse
import hashlib
import zipfile
from check_gd77_image import validate
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
p.add_argument("--donor-zip",type=Path,help="extract the provided GD-77 V4.3.6 donor for manual CPS merge")
p.add_argument("--lto", action="store_true", help="use whole-program optimization")
p.add_argument("--link", action="store_true", help="link/check BIN and generate CPS ZIP; CPS must merge the donor")
a = p.parse_args()
if a.donor_zip and not a.link:p.error("--donor-zip requires --link")
donor=None
donor_sha="d8bdfcc4c5fbb5e1f58e2d6f243381c673d6d09d4718b1ce9178e50fab01706f"
if a.donor_zip:
    member="Firmware software V4.3.6/GD-77_V4.3.6.sgl"
    with zipfile.ZipFile(a.donor_zip) as archive:
        if archive.getinfo(member).file_size!=494698:sys.exit("Unexpected donor length")
        donor=archive.read(member) # ZIP CRC checked, only this member is read
    if donor[:4]!=b"SGL!" or hashlib.sha256(donor).hexdigest()!=donor_sha:
        sys.exit("Donor mismatch: expected the supplied GD-77 V4.3.6 file")
source = a.source.resolve()
out = a.out.resolve(); out.mkdir(parents=True, exist_ok=True)
revision=subprocess.check_output(["git","-C",str(source),"rev-parse","HEAD"],text=True).strip()
dirty=(subprocess.run(["git","-C",str(source),"diff","--quiet","HEAD"]).returncode!=0 or
       bool(subprocess.check_output(["git","-C",str(source),"ls-files","--others","--exclude-standard","--","firmware","tools"],text=True).strip()))
build_epoch=subprocess.check_output(["git","-C",str(source),"show","-s","--format=%ct","HEAD"],text=True).strip()
build_env=os.environ.copy();build_env["SOURCE_DATE_EPOCH"]=build_epoch
version=revision[:8]+("+d" if dirty else "")
stem="OpenGD77-MDC1200-EXP" if a.experimental_mdc_rf else "OpenGD77-PTT-TalkPermit"
if a.link:
    for name in [stem+".elf",stem+".bin",stem+".zip",stem+".zip.unchecked","package-manifest.json","manifest.json","image-check.json"]:
        (out/name).unlink(missing_ok=True)
cc = shutil.which(a.cc)
if not cc: sys.exit("BLOCKED: arm-none-eabi-gcc missing")
firmware = source/"firmware"
for name in ("linkerscripts",):
    dest=out/name
    if not dest.exists(): dest.symlink_to(firmware/name, target_is_directory=True)
work=out/"build"; work.mkdir(exist_ok=True)
inc=["-I"+str(firmware)] + ["-I"+str(d) for d in sorted(firmware.rglob("*")) if d.is_dir()]
flags=["-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard",
       "-Os", "-DNDEBUG", "-g3", "-std=gnu11", "-ffunction-sections", "-fdata-sections", "-fno-common",
       "-DPLATFORM_GD77", "-DGD77_NEWLIB_BUILD=1", "-DHAS_GPS=1", "-DCPU_MK22FN512VLL12", "-DSDK_OS_FREE_RTOS", "-D__USE_CMSIS",
       '-DGITVERSION="'+version+'"', "-Werror=implicit-function-declaration", *inc]
if a.experimental_mdc_rf:
    flags += ["-DGD77_EXPERIMENTAL_MDC_RF=1",
              "-I"+str(source/"tools/mdc1200")]
if a.lto: flags += ["-flto"]
if a.newlib: flags += ["-isystem", str(a.newlib.resolve()/"include/newlib/nano"), "-isystem", str(a.newlib.resolve()/"include/newlib")]
files=sorted(firmware.rglob("*.c"))
# Segger syscalls are optional debugger/Redlib integration, not newlib runtime.
files=[f for f in files if "/SeggerRTT/" not in str(f) and "/include/" not in str(f)
       and not (f.name.startswith("heap_") and f.name != "heap_4.c")]
if a.experimental_mdc_rf:files.append(source/"tools/mdc1200/mdc1200_codec.c")
commands=[]
def compile_one(f):
    relative=f.relative_to(firmware) if f.is_relative_to(firmware) else f.relative_to(source)
    obj=work/(str(relative).replace("/","_")+".o")
    # Fixed pre-codec startup must not absorb SystemInit/main via LTO inlining.
    local_flags=flags+["-fno-lto"] if f.name in ("startup_mk22f51212.c", "calibration.c", "menuCalibration.c") else flags
    cmd=[cc,*local_flags,"-c",str(f),"-o",str(obj)]
    result=subprocess.run(cmd,cwd=work,capture_output=True,text=True,env=build_env)
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
(out/"commands.json").write_text(json.dumps(dict(common=[cc,*flags],
    jobs=[cmd[len(flags)+1:] for cmd in commands]),indent=2)+"\n")
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
elf=out/(stem+".elf")
cmd=[cc,*flags[:4],"-nostdlib","-Wl,--gc-sections", "-Wl,-Map="+str(out/"GD77.map"),
     "-T",str(firmware/"linkerscripts/firmware_newlib.ld"), *map(str,objects)]
if a.newlib:cmd += ["-L"+str(a.newlib.resolve()/"lib/arm-none-eabi/newlib/thumb/v7e-m+fp/hard")]
if a.lto: cmd += ["-flto", "-Os"]
if a.experimental_mdc_rf:cmd += ["-Wl,-u,mdcRFDiagnostics"]
cmd += ["-o",str(elf)]
r=subprocess.run(cmd,cwd=work,capture_output=True,text=True,env=build_env)
(out/"link-command.json").write_text(json.dumps(cmd,indent=2)+"\n")
(out/"link.log").write_text(r.stdout+r.stderr)
if r.returncode:
    elf.unlink(missing_ok=True)
    sys.exit("BLOCKED: link failed; see link.log")
size=str(Path(cc).with_name("arm-none-eabi-size"))
summary=subprocess.check_output([size,str(elf)],text=True)
(out/"size.log").write_text(summary)
print(summary)
objcopy=str(Path(cc).with_name("arm-none-eabi-objcopy"))
pending_bin=out/(stem+".bin.unchecked")
subprocess.run([objcopy,"-O","binary","--gap-fill","0xff",str(elf),str(pending_bin)],check=True)
try:
    image_check=validate(elf.read_bytes(),pending_bin.read_bytes(),a.experimental_mdc_rf)
except Exception:
    pending_bin.unlink(missing_ok=True)
    raise
binary=out/(stem+".bin");pending_bin.replace(binary);binary.chmod(0o644)
(out/"image-check.json").write_text(json.dumps(image_check,indent=2)+"\n")
manifest=dict(source_revision=revision,tracked_dirty=dirty,source_date_epoch=int(build_epoch),
              firmware=binary.name,experimental_mdc_rf=a.experimental_mdc_rf,fully_linked=True,donor_required=True,donor_version="GD-77 V4.3.6",
              **image_check)
if donor is not None:
    (out/"GD-77_V4.3.6.sgl").write_bytes(donor)
    manifest["donor"]=dict(file="GD-77_V4.3.6.sgl",sha256=donor_sha,bytes=len(donor),zip_crc_checked=True)
(out/"manifest.json").write_text(json.dumps(manifest,indent=2)+"\n")
(out/"README.txt").write_text(
    "GD-77 EXPERIMENTAL firmware. Hardware boot, CPS roundtrip and MDC RF decode: NOT RUN.\n"
    "Use OpenGD77 CPS E2026.05.26.01 Extras / Firmware Loader, select GD-77.\n"
    "Select official GD-77_V4.3.6.sgl as DONOR, then select "+binary.name+" as OpenGD77 firmware.\n"
    "The BIN contains empty codec slots; CPS merges the donor before upload.\n"
    "Do not flash the donor itself or use this ELF/BIN via raw SWD. No auto flash was performed.\n"
    "PTT Tone defaults Off; choose MDC1200 EXP explicitly for RF tests.\n"
    "This GNU release uses newlib-nano/LTO; radio verification remains required.\n"
    "Donor material and combined build are private local artifacts, not committed/distributed by this task.\n")
print("PASS image layout and vectors:",binary)
print("BIN SHA256:",image_check["sha256"])
print("Required in CPS: GD-77 V4.3.6 donor selected separately. No device action performed.")

# Packaging is a required final build gate, using the same source tree as BIN.
subprocess.run([sys.executable, str(ROOT/"tools/package_gd77_zip.py"),
                "--artifacts", str(out), "--source", str(source)], check=True)
print("CPS ZIP:", out/(stem+".zip"))
