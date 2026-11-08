#!/usr/bin/env python3
"""External-decoder checks of delay/phase HYPOTHESES, never RF measurements."""
import ctypes as c
import json
import math
from pathlib import Path
import random
import subprocess
import sys
import tempfile
ROOT=Path(__file__).resolve().parents[2]
ref=Path(sys.argv[1]).resolve()
assert subprocess.check_output(['git','-C',str(ref),'rev-parse','HEAD'],text=True).strip()=='45fc9095fbde8bf6a6988c7dc0de64a4ec0ffe12'
assert not subprocess.check_output(['git','-C',str(ref),'status','--porcelain'],text=True).strip()
golden=json.loads((ROOT/'tests/feature_migration/mdc_golden.json').read_text())
with tempfile.TemporaryDirectory() as td:
    td=Path(td);source=td/'decoder.c'
    source.write_text('''#include <stdlib.h>
#include "mdc_decode.c"
int decode(short *s,int n,int *out) {
 mdc_decoder_t *d=mdc_decoder_new(48000);if(!d)return -1;
 int r=mdc_decoder_process_samples(d,s,n);
 unsigned char op=255,arg=255;unsigned short id=0;
 if(r==1 && mdc_decoder_get_packet(d,&op,&arg,&id)==0){out[0]=op;out[1]=arg;out[2]=id;}
 free(d);return r;
}
''')
    so=td/'decoder.so'
    subprocess.run(['gcc','-shared','-fPIC','-I'+str(ref),str(source),'-lm','-o',str(so)],check=True)
    lib=c.CDLL(str(so));lib.decode.argtypes=[c.POINTER(c.c_int16),c.c_int,c.POINTER(c.c_int)]
    results=[]
    for profile,reset_phase in [('continuous_phase_delayed_write',False),('reset_phase_on_frequency_change',True)]:
        for v in golden['vectors']:
            data=bytes.fromhex(v['tone_bits'])
            tones=[(data[n//8]>>(7-n%8))&1 for n in range(208)]
            rng=random.Random(int(v['unit_id'],16))
            # 90us wire time + bounded 0..25us scheduling/completion variation.
            changes={n*40+round((90+rng.randrange(26))*48000/1000000):tones[n]
                     for n in range(1,208) if tones[n]!=tones[n-1]}
            phase=0.;tone=tones[0];pcm=[]
            for n in range(8320):
                if n in changes:
                    tone=changes[n]
                    if reset_phase:phase=0.
                pcm.append(round(12000*math.sin(phase)))
                phase=(phase+2*math.pi*(1800 if tone else 1200)/48000)%(2*math.pi)
            pcm=[0]*2400+pcm+[0]*2400
            out=(c.c_int*3)();r=lib.decode((c.c_int16*len(pcm))(*pcm),len(pcm),out)
            expected=[1,0,int(v['unit_id'],16)]
            passed=r==1 and list(out)==expected
            results.append(dict(profile=profile,unit_id=v['unit_id'],decoded=passed,raw=list(out),frames=r))
            if not reset_phase:assert passed,(profile,v['unit_id'],r,list(out))
    print(json.dumps(dict(kind='SYNTHETIC HYPOTHESIS, NOT RF EVIDENCE',results=results),indent=2))
