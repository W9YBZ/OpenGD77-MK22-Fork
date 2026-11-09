# SPDX-License-Identifier: GPL-2.0-only
"""Build external oracles only. Never imports/builds the implementation under test."""
from pathlib import Path
import ctypes as C,subprocess,sys
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'host/mdc1200'))
from fetch_references import verify,LOCK
U8=C.c_uint8;U16=C.c_uint16
class Oracles:
    def __init__(self,refs,build):
        refs=Path(refs).resolve();build=Path(build);build.mkdir(parents=True,exist_ok=True);verify(refs)
        for name in ['encode','decode']:
            so=build/('oracle_'+name+'.so')
            subprocess.run(['gcc','-std=c11','-O2','-fPIC','-shared','-I'+str(refs/'kaufman'),str(ROOT/'tests/mdc1200'/('oracle_'+name+'.c')),'-lm','-o',str(so)],check=True)
            setattr(self,name,C.CDLL(str(so)))
        self.encode.oracle_crc.argtypes=[C.POINTER(U8),C.c_uint];self.encode.oracle_crc.restype=U16
        self.encode.oracle_encode.argtypes=[U8,U8,U16,C.POINTER(U8),C.POINTER(U16),C.POINTER(U8)]
        self.decode.oracle_decode_block.argtypes=[C.POINTER(U8),C.POINTER(U16)]
        self.decode.oracle_decode_pcm.argtypes=[C.POINTER(C.c_int16),C.c_int,C.POINTER(U16)]
    def vector(self,op,arg,unit):
        frame=(U8*26)();states=(U16*4)();tones=(U8*26)()
        assert self.encode.oracle_encode(op,arg,unit,frame,states,tones)==0
        frame=bytes(frame)
        # Independently inverse-map the external frame by the audited decoder's
        # 16-column / 7-row geometry. No new-encoder algorithm is called.
        bits=[(b>>i)&1 for b in frame[12:] for i in range(7,-1,-1)]
        linear=[bits[row*16+col] for col in range(16) for row in range(7)]
        fec=bytes(sum(linear[n*8+j]<<j for j in range(8)) for n in range(14))
        return dict(op=f'{op:02X}',arg=f'{arg:02X}',unit_id=f'{unit:04X}',crc_prefix_raw=[f'{n:04X}' for n in states],crc=f'{fec[5]:02X}{fec[4]:02X}',info=fec[:7].hex().upper(),fec=fec.hex().upper(),logical_frame=frame.hex().upper(),tone_bits=bytes(tones).hex().upper(),frame_bytes=26,symbols=208)
