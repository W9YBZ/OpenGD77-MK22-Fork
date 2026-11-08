#!/usr/bin/env python3
"""External oracle only. Never imports/calls the implementation under test."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
REF = "45fc9095fbde8bf6a6988c7dc0de64a4ec0ffe12"
ref = Path(sys.argv[1]).resolve()
assert subprocess.check_output(["git", "-C", str(ref), "rev-parse", "HEAD"], text=True).strip() == REF
assert not subprocess.check_output(["git", "-C", str(ref), "status", "--porcelain"], text=True).strip()
# Include pristine encoder to expose its static CRC, not a reimplementation.
helper = r'''
#include <stdio.h>
#include <string.h>
#include "mdc_encode.c"
int main(void) {
 unsigned ids[] = {1, 0x1234, 0xffff};
 for (unsigned x=0; x<3; x++) {
  mdc_encoder_t *e=mdc_encoder_new(48000);
  mdc_encoder_set_packet(e,1,0,ids[x]);
  printf("%04X ",ids[x]);
  unsigned char d[]={1,0,ids[x]>>8,ids[x]&255};
  for(int n=1;n<=4;n++)printf("%04X",_docrc(d,n)^0xffff);
  printf(" ");
  for(int n=0;n<26;n++)printf("%02X",e->data[n]);
  printf(" ");
  unsigned char tones[26]={0};
  mdc_sample_t sample;
  int previous=-1;
  while(mdc_encoder_get_samples(e,&sample,1)>0) {
   int symbol=e->bpos*8+e->ipos;
   if(symbol>=208)break;
   if(symbol!=previous) {
    if(e->xorb)tones[symbol/8]|=1<<(7-symbol%8);
    previous=symbol;
   }
  }
  for(int n=0;n<26;n++)printf("%02X",tones[n]);
  puts(""); free(e);
 }
}
'''
with tempfile.TemporaryDirectory() as td:
    source = Path(td)/"oracle.c"
    source.write_text(helper)
    exe = Path(td)/"oracle"
    subprocess.run(["gcc", "-std=c11", "-I"+str(ref), str(source), "-o", str(exe)], check=True)
    lines = subprocess.check_output([str(exe)], text=True).splitlines()
vectors=[]
for line in lines:
    unit, states, frame_hex, tones = line.split()
    raw = bytes.fromhex(frame_hex)
    # Independently unpack oracle's over-air payload using decoder's matrix.
    wire_bits = [(b >> j)&1 for b in raw[12:] for j in range(7,-1,-1)]
    linear = [wire_bits[row*16+col] for col in range(16) for row in range(7)]
    block = bytes(sum(linear[i*8+j]<<j for j in range(8)) for i in range(14))
    vectors.append(dict(unit_id=unit, crc_states_before_xor=[states[i:i+4] for i in range(0,16,4)],
                        crc=f"{block[5]:02X}{block[4]:02X}", fec_block=block.hex().upper(),
                        frame=frame_hex, tone_bits=tones, length_bytes=len(raw), symbols=208))
print(json.dumps(dict(source="https://github.com/atmatthewat/mdc-encode-decode",commit=REF,
                     profile="candidate ANI/EOT op=01 arg=00; RF unverified",vectors=vectors), indent=2))
