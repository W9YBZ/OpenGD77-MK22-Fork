#!/usr/bin/env python3
"""Validate and package a completed official MCUXpresso GD-77 build. No flashing."""
import argparse,hashlib,json,re,shutil,subprocess,tempfile,zipfile
from datetime import datetime,timezone
from pathlib import Path
from check_native_gd77 import validate_native,elf_details
ROOT=Path(__file__).resolve().parents[1]

def package(source,out,name,donor_zip=None):
    source=Path(source).resolve();out=Path(out).resolve();out.mkdir(parents=True,exist_ok=True)
    donor=None
    if donor_zip:
        with zipfile.ZipFile(donor_zip) as archive:
            member='Firmware software V4.3.6/GD-77_V4.3.6.sgl'
            assert archive.getinfo(member).file_size==494698
            donor=archive.read(member)
        digest=hashlib.sha256(donor).hexdigest()
        assert digest=='d8bdfcc4c5fbb5e1f58e2d6f243381c673d6d09d4718b1ce9178e50fab01706f'
    build=source/'firmware/GD-77';elf=build/'firmware.axf';binary=build/'OpenGD77.bin'
    data=binary.read_bytes();raw=(build/'firmware.bin').read_bytes();assert raw==data,'unexpected codec-cleaner changes'
    check=validate_native(elf.read_bytes(),data)
    revision=subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip()
    short=subprocess.check_output(['git','-C',str(source),'rev-parse','--short','HEAD'],text=True).strip()
    assert short.encode()+b'\0' in data,'image does not contain this source revision'
    epoch=int(subprocess.check_output(['git','-C',str(source),'show','-s','--format=%ct','HEAD'],text=True))
    lang=source/'firmware/include/user_interface/languages'
    builder_source=lang/'src/languages_builder.c'
    names=re.search(r'const char \*languageEnglishNames\[\]\s*=\s*\{(.*?)\};',builder_source.read_text(),re.S).group(1)
    expected={n+'.gla' for n in re.findall(r'"([^"]+)"',names)}
    _,_,symbols=elf_details(elf.read_bytes());pos=symbols['languages']-0x4000
    signature=data[pos:pos+12];assert signature==b'GD77LANG\0\0\0\x05'
    payload={'OpenGD77.bin':data}
    with tempfile.TemporaryDirectory(prefix='gd77-native-languages-') as temp:
        temp=Path(temp);builder=temp/'builder'
        subprocess.run(['gcc','-Wall','-O2','-I'+str(lang),str(builder_source),'-o',str(builder)],check=True)
        run=subprocess.run([str(builder)],cwd=temp,capture_output=True,text=True,check=True)
        (out/'languages.log').write_text(run.stdout+run.stderr)
        gla={p.name:p for p in temp.glob('*.gla')};assert set(gla)==expected
        for entry,path in sorted(gla.items()):
            value=path.read_bytes();assert len(value)==4772 and value[:12]==signature
            payload[entry]=value
    pending=out/(name+'.zip.unchecked')
    with zipfile.ZipFile(pending,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9,allowZip64=False) as archive:
        for entry,value in payload.items():
            info=zipfile.ZipInfo(entry,datetime.fromtimestamp(epoch,timezone.utc).timetuple()[:6]);info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o100644<<16
            archive.writestr(info,value,compresslevel=9)
    with zipfile.ZipFile(pending) as archive:
        assert archive.testzip() is None and set(archive.namelist())==set(payload)
        assert archive.read('OpenGD77.bin')==data and not any(n.endswith('.sgl') for n in archive.namelist())
    target=out/(name+'.zip');pending.replace(target)
    for src,suffix in [(binary,'.bin'),(elf,'.axf'),(build/'firmware.map','.map')]:shutil.copyfile(src,out/(name+suffix))
    result=dict(source_revision=revision,build='Official GD-77 MCUXpresso preset / Redlib; no LTO',
                file=target.name,zip_sha256=hashlib.sha256(target.read_bytes()).hexdigest(),zip_bytes=target.stat().st_size,
                language_count=len(expected),zip_crc='PASS',donor_required=True,donor_in_zip=False,
                hardware_boot='NOT_RUN',hardware_audio='NOT_RUN',cps_roundtrip='NOT_RUN',**check)
    if donor is not None:
        (out/'GD-77_V4.3.6.sgl').write_bytes(donor)
        result['donor_sha256']=digest
    (out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n')
    (out/'README.txt').write_text('Native MCUXpresso GD-77 package: '+target.name+'\nSource: '+revision+'\nSelect GD-77_V4.3.6.sgl separately as donor in CPS, then select this ZIP.\nHardware boot/audio/CPS acceptance NOT RUN. Test baseline key beeps before Talk Permit.\nNo flashing was performed. Do not use this placeholder BIN/AXF via raw SWD.\n')
    print(json.dumps(result,indent=2));return result

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,default=ROOT);p.add_argument('--out',type=Path,required=True);p.add_argument('--name',required=True);p.add_argument('--donor-zip',type=Path)
    a=p.parse_args();package(a.source,a.out,a.name,a.donor_zip)
