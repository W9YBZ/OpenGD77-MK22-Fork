#!/usr/bin/env python3
"""Validate actual ELF32 load sections and CPS BIN; no device access."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

FLASH_BASE=0x4000
UPLOAD_END=0x7f000
CODECS={'.codec_bin_section_1':(0x4400,0xf00),'.codec_bin_section_2':(0x54000,0x28090)}

def elf_details(data):
    assert data[:7]==b'\x7fELF\x01\x01\x01','not little-endian ELF32'
    h=struct.unpack_from('<HHIIIIIHHHHHH',data,16)
    kind,machine,_,entry,phoff,shoff,flags,_,phsz,phnum,shsz,shnum,stridx=h
    assert kind==2 and machine==40 and (flags&0x600)==0x400,'wrong ARM executable/float ABI'
    assert phsz==32 and shsz==40
    raw=[struct.unpack_from('<IIIIIIIIII',data,shoff+n*shsz) for n in range(shnum)]
    names=data[raw[stridx][4]:raw[stridx][4]+raw[stridx][5]]
    def string(table,offset):return table[offset:table.index(b'\0',offset)].decode()
    sections=[]
    for row in raw:
        name,typ,flg,addr,off,size,link,info,align,entsize=row
        sections.append(dict(name=string(names,name),type=typ,flags=flg,address=addr,
                             offset=off,size=size,link=link,entry_size=entsize))
    ph=[struct.unpack_from('<IIIIIIII',data,phoff+n*phsz) for n in range(phnum)]
    symbols={}
    for s in sections:
        if s['type']!=2:continue
        strings=sections[s['link']]
        table=data[strings['offset']:strings['offset']+strings['size']]
        for pos in range(s['offset'],s['offset']+s['size'],s['entry_size']):
            name,value,size,info,other,ndx=struct.unpack_from('<IIIBBH',data,pos)
            if ndx and name:symbols[string(table,name)]=value
    for s in sections:
        if not (s['flags']&2) or not s['size']:continue
        if s['type']==8:continue
        for typ,off,va,pa,filesz,memsz,flg,align in ph:
            if typ==1 and off<=s['offset'] and s['offset']+s['size']<=off+filesz:
                s['load_address']=pa+s['offset']-off
                break
        assert 'load_address' in s,('allocated contents lack LOAD segment',s['name'])
    return entry,sections,symbols

def validate(elf_bytes,image,experimental=False):
    entry,sections,syms=elf_details(elf_bytes)
    byname={s['name']:s for s in sections}
    for name,(address,size) in CODECS.items():
        s=byname[name]
        assert s['address']==s.get('load_address')==address and s['size']==size,(name,'moved/missing ALLOC')
        assert elf_bytes[s['offset']:s['offset']+size]==b'\xff'*size,(name,'unexpected donor/placeholder contents')
    assert byname['.text_start']['address']==FLASH_BASE
    assert byname['.text_start']['address']+byname['.text_start']['size']<=0x4400
    assert byname['.text_main']['address']>=0x5300
    assert byname['.upper_text']['address']==0x7c090
    flash=[];ram=[]
    for s in sections:
        if not (s['flags']&2) or not s['size']:continue
        lo,hi=s['address'],s['address']+s['size']
        assert FLASH_BASE<=lo<hi<=UPLOAD_END or 0x1fff0000<=lo<hi<=0x20000000 or 0x20000000<=lo<hi<=0x20010000,('section out of memory',s)
        if lo>=0x1fff0000:ram.append((lo,hi,s['name']))
        if s['type']==8:continue
        lo=s['load_address'];hi=lo+s['size']
        assert FLASH_BASE<=lo<hi<=UPLOAD_END,('load outside application flash',s)
        if s['name'] not in CODECS:
            for start,size in CODECS.values():assert hi<=start or lo>=start+size,('overlap codec',s)
        flash.append((lo,hi,s['name']))
        start=lo-FLASH_BASE
        assert image[start:start+s['size']]==elf_bytes[s['offset']:s['offset']+s['size']],('BIN/ELF mismatch',s['name'])
    for group in (flash,ram):
        ordered=sorted(group)
        for a,b in zip(ordered,ordered[1:]):assert a[1]<=b[0],('section overlap',a,b)
    assert len(image)==max(b for a,b,n in flash)-FLASH_BASE==syms['_image_size']
    assert syms['_image_end']==FLASH_BASE+len(image)
    sp,pc=struct.unpack_from('<II',image)
    assert sp==syms['_vStackTop']==0x20000000 and sp%8==0
    assert pc==entry and pc&1 and (pc&~1)==(syms['ResetISR']&~1)
    assert 0x4000<=(pc&~1)<0x4400
    assert 0x1fff0000<=syms['_pvHeapStart']<syms['_pvHeapLimit']<=syms['_vStackBase']<sp
    assert syms['_pvHeapLimit']-syms['_pvHeapStart']==4096 and sp-syms['_vStackBase']==4096
    # Actual vector contents must select the strong experimental IRQ handlers.
    if experimental:
        for symbol in ('PIT1_IRQHandler','I2C0_IRQHandler'):
            assert symbol in syms,('missing RF implementation',symbol)
        vectors=image[:byname['.text_start']['size']]
        for symbol in ('PIT1_IRQHandler','I2C0_IRQHandler'):
            pointer=(syms[symbol]&~1)|1
            assert struct.pack('<I',pointer) in vectors,('RF handler absent from vector table',symbol)
    return dict(bytes=len(image),sha256=hashlib.sha256(image).hexdigest(),flash_base=FLASH_BASE,
                flash_end=FLASH_BASE+len(image),upload_headroom=UPLOAD_END-FLASH_BASE-len(image),
                initial_sp=sp,reset_vector=pc,codec_regions={k:dict(address=a,size=n) for k,(a,n) in CODECS.items()},
                heap_start=syms['_pvHeapStart'],heap_limit=syms['_pvHeapLimit'],stack_base=syms['_vStackBase'],
                ram_sections=[dict(start=a,end=b,name=n) for a,b,n in sorted(ram)],
                codec_merge='required in CPS using GD-77_V4.3.6.sgl',rf_validation='NOT_RUN')

def main():
    p=argparse.ArgumentParser();p.add_argument('elf',type=Path);p.add_argument('bin',type=Path)
    p.add_argument('--experimental-mdc-rf',action='store_true');a=p.parse_args()
    print(json.dumps(validate(a.elf.read_bytes(),a.bin.read_bytes(),a.experimental_mdc_rf),indent=2))
if __name__=='__main__':main()
