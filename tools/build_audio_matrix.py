#!/usr/bin/env python3
"""Build read-only audio isolation packages; never flash or access a radio."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
BASELINE='72825f29d19729b0de3ec2711351a2845dbcff91'
# These are explicit GNU/runtime/boot support and common audio diagnostic fixes,
# not the FM feature handlers, settings encoding or MDC encoder/modulator.
OVERLAY=(
    'firmware/linkerscripts/firmware_newlib.ld',
    'firmware/source/dmr_codec/codec_bin.S',
    'firmware/source/runtime/newlib_heap.c',
    'firmware/source/hardware/SPI_Flash.c',
    'firmware/include/FreeRTOSConfig.h',
    'firmware/include/functions/sound.h',
    'firmware/source/functions/sound.c',
    'firmware/source/main.c',
    'firmware/source/user_interface/menuFirmwareInfoScreen.c',
)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--cc',required=True)
    p.add_argument('--newlib',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--donor-zip',type=Path)
    a=p.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    revision=subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip()
    subprocess.run(['git','-C',str(ROOT),'diff','--exit-code','HEAD','--','firmware','tools'],check=True)
    common=[sys.executable,str(ROOT/'tools/build_gd77.py'),'--cc',a.cc,
            '--newlib',str(a.newlib.resolve()),'--lto','--link']
    if a.donor_zip:common+=['--donor-zip',str(a.donor_zip.resolve())]
    results=[]
    def build(name,source,options,overlay=None):
        folder=out/name;folder.mkdir(exist_ok=True)
        cmd=common+['--source',str(source),'--out',str(folder)]+options
        print('Building '+name,flush=True)
        with (folder/'build.log').open('w') as log:
            subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,check=True)
        result=dict(variant=name,source_revision=revision,baseline=BASELINE if overlay else None,
                    common_audio_fixes=True,overlay=overlay,command=cmd,
                    hardware_audio='NOT_RUN',manifest=json.loads((folder/'manifest.json').read_text()),
                    package=json.loads((folder/'package-manifest.json').read_text()))
        (folder/'isolation.json').write_text(json.dumps(result,indent=2)+'\n')
        results.append(result)
        (out/'matrix.json').write_text(json.dumps(results,indent=2)+'\n')
        print('PASS linked image + CPS ZIP: '+name,flush=True)
    # Native repository source with no custom FM handlers/settings, retaining
    # the proven Flash fix and identical common audio diagnostics/repair.
    with tempfile.TemporaryDirectory(prefix='gd77-audio-base-') as tmp:
        tree=Path(tmp)/'source'
        subprocess.run(['git','-C',str(ROOT),'worktree','add','--detach',str(tree),BASELINE],check=True)
        try:
            provenance={}
            for name in OVERLAY:
                data=(ROOT/name).read_bytes()
                if name=='firmware/source/functions/sound.c':
                    data=data.replace(b'#if defined(PLATFORM_GD77)\nconst int16_t MELODY_FM_TALK_PERMIT[] = { 940, 40, 0, 10, 940, 40, 0, 10, 940, 40, -1, -1 };\n#endif\n',b'')
                target=tree/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
                provenance[name]=hashlib.sha256(data).hexdigest()
            # Build script is taken from ROOT, not an older baseline build tool.
            build('base-audio',tree,[],provenance)
            # Keep the identical linker script symlink usable after temp cleanup.
            (out/'base-audio/linkerscripts').unlink()
            (out/'base-audio/linkerscripts').symlink_to(ROOT/'firmware/linkerscripts',target_is_directory=True)
        finally:
            subprocess.run(['git','-C',str(ROOT),'worktree','remove','--force',str(tree)],check=True)
    build('ptt-permit',ROOT,[])
    build('full-lto',ROOT,['--experimental-mdc-rf'])
    build('full-audio-no-lto',ROOT,['--experimental-mdc-rf','--no-lto-audio'])
    print('All 4 packages checked. Hardware audio/RF acceptance NOT RUN.',flush=True)

if __name__=='__main__':main()
