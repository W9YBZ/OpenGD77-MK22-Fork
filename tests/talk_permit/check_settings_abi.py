#!/usr/bin/env python3
"""Compare every settings field against baseline using the target compiler ABI."""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser()
p.add_argument("--baseline",type=Path,required=True)
p.add_argument("--cc",required=True)
p.add_argument("--newlib",type=Path)
a=p.parse_args()
cc=shutil.which(a.cc)
objcopy=str(Path(cc).with_name("arm-none-eabi-objcopy"))
with tempfile.TemporaryDirectory() as td:
    td=Path(td)
    for enum_flags in ([],["-fno-short-enums"]):
        blobs=[]
        for label,repo in [("baseline",a.baseline.resolve()),("feature",ROOT)]:
            fw=repo/"firmware"
            inc=["-I"+str(fw)]+["-I"+str(d) for d in fw.rglob("*") if d.is_dir()]
            flags=["-mcpu=cortex-m4","-mthumb","-std=gnu11","-DPLATFORM_GD77",
                   "-DCPU_MK22FN512VLL12","-DSDK_OS_FREE_RTOS","-DHAS_GPS=1",*enum_flags,*inc]
            if a.newlib:flags += ["-isystem",str(a.newlib.resolve()/"include/newlib")]
            src=td/"probe.c"
            prefix='#include "main.h"\n#include "functions/settings.h"\n#include <stddef.h>\n'
            src.write_text(prefix)
            text=subprocess.check_output([cc,*flags,"-E","-P",str(src)],text=True)
            body=re.search(r'typedef struct\s*\{([^{}]*)\}\s*settingsStruct_t;',text).group(1)
            names=re.findall(r'\b(\w+)\s*(?:\[[^;]+\])?\s*;',body)
            values=['sizeof(settingsStruct_t)','_Alignof(settingsStruct_t)']
            for name in names:values += [f'offsetof(settingsStruct_t,{name})',f'sizeof(((settingsStruct_t *)0)->{name})']
            src.write_text(prefix+'const unsigned settings_layout[] = {'+','.join(values)+'};\n')
            obj=td/(label+'.o');raw=td/(label+'.dat')
            subprocess.run([cc,*flags,'-fdata-sections','-c',str(src),'-o',str(obj)],check=True)
            subprocess.run([objcopy,'-O','binary','--only-section=.rodata.settings_layout',str(obj),str(raw)],check=True)
            blobs.append(raw.read_bytes())
        assert blobs[0] == blobs[1], "settings ABI changed: "+str(enum_flags)
        import struct
        values=struct.unpack('<'+'I'*(len(blobs[1])//4),blobs[1])
        fields={name:dict(offset=values[2+2*i],size=values[3+2*i]) for i,name in enumerate(names)}
        print(json.dumps(dict(enum_flags=enum_flags,settings_size=values[0],alignment=values[1],fields=fields),indent=2))
        print('PASS: baseline/feature byte-identical layout for all',len(names),'fields')
