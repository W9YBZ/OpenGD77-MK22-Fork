#!/usr/bin/env python3
"""Mutation tests on real native ELF/BIN outputs, never hardware."""
from pathlib import Path
import struct,sys
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
from check_native_gd77 import validate_native,elf_details
elf=Path(sys.argv[1]).read_bytes();binary=Path(sys.argv[2]).read_bytes()
validate_native(elf,binary)
def rejected(e,b):
 try:validate_native(e,b)
 except AssertionError:return
 raise AssertionError('corrupt image accepted')
for offset in [0,4,11*4,(16+48)*4,0x400,0x50000,len(binary)-1]:
 bad=bytearray(binary);bad[offset]^=1;rejected(elf,bad)
rejected(elf,binary[:-1]);rejected(elf,binary+b'\xff')
_,sections,_=elf_details(elf);h=struct.unpack_from('<HHIIIIIHHHHHH',elf,16)
for name in ['.text_main','.codec_bin_section_1','.codec_bin_section_2']:
 idx=next(i for i,s in enumerate(sections) if s['name']==name)
 bad=bytearray(elf);struct.pack_into('<I',bad,h[5]+idx*h[10]+8,0)
 rejected(bad,binary)
print('PASS: native layout plus corrupt vectors/IRQs/codec/data, truncated/extended images and missing ALLOC rejected')
