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

def validate_native(elf,image):
    entry,sections,syms=elf_details(elf)
    byname={s['name']:s for s in sections}
    for name in ['.text_start','.text_main','.upper_text']:
        assert byname[name]['flags']&6==6,('code not allocated/executable',name)
    assert byname['.data']['flags']&3==3 and byname['.bss']['type']==8
    flash=[];ram=[]
    for name,(address,size) in CODECS.items():
        s=byname[name]
        # The official script includes leading padding in these output sections.
        assert s['flags']&2 and s['address']==s['load_address']<=address
        assert s['address']+s['size']==address+size
        assert image[address-FLASH_BASE:address-FLASH_BASE+size]==b'\xff'*size
    for s in sections:
        if not(s['flags']&2) or not s['size']:continue
        lo=s['address'];hi=lo+s['size']
        if lo>=0x1fff0000:
            assert 0x1fff0000<=lo<hi<=0x20000000 or 0x20000000<=lo<hi<=0x20010000
            ram.append((lo,hi,s['name']))
        else:assert FLASH_BASE<=lo<hi<=UPLOAD_END
        if s['type']==8:continue
        lo=s['load_address'];hi=lo+s['size'];assert FLASH_BASE<=lo<hi<=UPLOAD_END
        if s['name'] not in CODECS:
            for a,n in CODECS.values():assert hi<=a or lo>=a+n,('codec overlap',s['name'])
        assert image[lo-FLASH_BASE:hi-FLASH_BASE]==elf[s['offset']:s['offset']+s['size']],s['name']
        flash.append((lo,hi,s['name']))
    for intervals in [flash,ram]:
        ordered=sorted(intervals)
        for a,b in zip(ordered,ordered[1:]):assert a[1]<=b[0],('overlap',a,b)
    assert byname['.text_start']['address']==FLASH_BASE
    assert byname['.text_start']['address']+byname['.text_start']['size']<=0x4400
    assert byname['.text_main']['address']==0x5300
    assert byname['.upper_text']['address']==0x7c090
    assert len(image)==syms['_image_size']==syms['_image_end']-FLASH_BASE
    assert max(b for a,b,n in flash)==syms['_image_end']
    sp,pc=struct.unpack_from('<II',image)
    assert sp==syms['_vStackTop']==0x20000000
    assert pc==entry==(syms['ResetISR']|1) and FLASH_BASE<=pc<0x4400
    assert syms['_pvHeapLimit']-syms['_pvHeapStart']==syms['_HeapSize']==4096
    assert syms['_vStackBase']==sp-syms['_StackSize'] and syms['_StackSize']==4096
    for lo,hi,name in ram:
        if lo<0x20000000:assert hi<=syms['_vStackBase'],('ISR stack overlap',name)
    assert syms['_pvHeapLimit']<=syms['_vStackBase']
    vectors={'SVC_Handler':11,'PendSV_Handler':14,'SysTick_Handler':15,
             'I2S0_Tx_IRQHandler':16+28,'I2S0_Rx_IRQHandler':16+29,
             'PIT0_IRQHandler':16+48,'PORTC_IRQHandler':16+61}
    for name,index in vectors.items():assert struct.unpack_from('<I',image,index*4)[0]==syms[name]|1,name
    precodec=max(hi for lo,hi,name in flash if name not in CODECS and hi<=0x54000)
    return dict(bytes=len(image),sha256=hashlib.sha256(image).hexdigest(),
                elf_sha256=hashlib.sha256(elf).hexdigest(),flash_end=syms['_image_end'],
                precodec_headroom=0x54000-precodec,upload_headroom=UPLOAD_END-syms['_image_end'],
                initial_sp=sp,reset_vector=pc,heap_start=syms['_pvHeapStart'],heap_limit=syms['_pvHeapLimit'],
                stack_base=syms['_vStackBase'],stack_top=sp,vector_handlers=vectors,
                codec_placeholders='FF at 0x4400/0x54000; CPS donor required',ram_sections=ram)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('elf',type=Path);p.add_argument('bin',type=Path);a=p.parse_args()
    print(json.dumps(validate_native(a.elf.read_bytes(),a.bin.read_bytes()),indent=2))
