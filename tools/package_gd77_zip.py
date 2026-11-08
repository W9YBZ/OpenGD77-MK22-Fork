#!/usr/bin/env python3
"""Pack the existing checked GD77 BIN + matching GLA files for CPS. No flashing."""
import argparse
from datetime import datetime,timezone
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import zipfile
from check_gd77_image import validate,elf_details
ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--artifacts',type=Path,required=True)
    a=p.parse_args();folder=a.artifacts.resolve()
    manifest=json.loads((folder/'manifest.json').read_text())
    binary=folder/manifest['firmware'];elf=binary.with_suffix('.elf')
    image=binary.read_bytes()
    info=validate(elf.read_bytes(),image,manifest['experimental_mdc_rf'])
    assert info['sha256']==manifest['sha256'],'BIN differs from recorded build'
    lang=ROOT/'firmware/include/user_interface/languages'
    subprocess.run(['git','-C',str(ROOT),'diff','--quiet',manifest['source_revision'],'--',str(lang)],check=True)
    # Match the native package.bat: one OpenGD77.bin and flat, optional GLA files.
    source=lang/'src/languages_builder.c'
    text=source.read_text()
    names=re.search(r'const char \*languageEnglishNames\[\]\s*=\s*\{(.*?)\};',text,re.S).group(1)
    expected={name+'.gla' for name in re.findall(r'"([^"]+)"',names)}
    _,_,symbols=elf_details(elf.read_bytes())
    signature=image[symbols['languages']-0x4000:symbols['languages']-0x4000+12]
    assert signature==b'GD77LANG\0\0\0\x05','unsupported firmware language schema'
    with tempfile.TemporaryDirectory(prefix='gd77-cps-zip-') as temp:
        temp=Path(temp);builder=temp/'languages_builder'
        cmd=['gcc','-Wall','-O2','-I'+str(lang),str(source),'-o',str(builder)]
        subprocess.run(cmd,check=True)
        r=subprocess.run([str(builder)],cwd=temp,capture_output=True,text=True,check=True)
        (folder/'languages-build.log').write_text(r.stdout+r.stderr)
        generated={f.name:f for f in temp.glob('*.gla')}
        assert set(generated)==expected,'language builder did not produce all expected files'
        payload={'OpenGD77.bin':image}
        for name,path in sorted(generated.items()):
            data=path.read_bytes()
            assert data[:12]==signature and len(data)==4772,(name,'language ABI mismatch')
            payload[name]=data
        stamp=datetime.fromtimestamp(manifest['source_date_epoch'],timezone.utc).timetuple()[:6]
        pending=folder/(binary.stem+'.zip.unchecked')
        with zipfile.ZipFile(pending,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9,allowZip64=False) as z:
            for name,data in payload.items():
                entry=zipfile.ZipInfo(name,stamp);entry.compress_type=zipfile.ZIP_DEFLATED
                entry.external_attr=0o100644<<16
                z.writestr(entry,data,compresslevel=9)
        with zipfile.ZipFile(pending) as z:
            assert z.testzip() is None
            assert set(z.namelist())==set(payload)
            assert z.read('OpenGD77.bin')==image
            assert sum(n.endswith('.bin') for n in z.namelist())==1
            assert not any(n.endswith('.sgl') or '/' in n for n in z.namelist())
        target=pending.with_suffix('');pending.replace(target)
        result=dict(file=target.name,sha256=hashlib.sha256(target.read_bytes()).hexdigest(),
                    bytes=target.stat().st_size,firmware_sha256=info['sha256'],
                    firmware_source=manifest['source_revision'],language_count=len(generated),
                    entries=list(payload),crc_check='PASS',bin_unchanged=True,donor_in_zip=False,
                    cps_hardware_test='NOT_RUN')
        (folder/'package-manifest.json').write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps(result,indent=2))
    readme=folder/'README.txt'
    text=readme.read_text()
    if 'CPS ZIP:' not in text:
        readme.write_text(text+'\nCPS ZIP: '+target.name+' (select as firmware package; contains OpenGD77.bin + 18 matching GLA files).\n'
                          'Select the official GD-77_V4.3.6.sgl separately as donor; it is NOT inside the ZIP.\n')
if __name__=='__main__':main()
