#!/usr/bin/env python3
"""Reject corrupt/misplaced/non-loadable images using actual linked artifacts."""
from pathlib import Path
import struct
import sys
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from check_gd77_image import validate,elf_details
elf=Path(sys.argv[1]).read_bytes();binary=Path(sys.argv[2]).read_bytes()
validate(elf,binary,True)
def rejected(e,b):
    try:validate(e,b,True)
    except AssertionError:return
    raise AssertionError('corrupt image passed validation')
for offset in [0,4,0x400,0x50000,len(binary)-1]:
    bad=bytearray(binary);bad[offset]^=1;rejected(elf,bad)
rejected(elf,binary[:-1]);rejected(elf,binary+b'\xff')
h=struct.unpack_from('<HHIIIIIHHHHHH',elf,16)
shoff,shsz=h[5],h[10]
_,sections,_=elf_details(elf)
for name in ['.codec_bin_section_1','.codec_bin_section_2']:
    index=next(i for i,s in enumerate(sections) if s['name']==name)
    bad=bytearray(elf);struct.pack_into('<I',bad,shoff+index*shsz+8,0)
    rejected(bad,binary)
index=next(i for i,s in enumerate(sections) if s['name']=='.text_main')
bad=bytearray(elf);struct.pack_into('<I',bad,shoff+index*shsz+12,0x4400)
rejected(bad,binary)
print('PASS: actual ELF/BIN; mutations rejected: vectors, codec/data corruption, truncation, length, missing ALLOC and codec overlap')
