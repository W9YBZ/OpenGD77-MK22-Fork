#!/usr/bin/env python3
"""Golden-layer tests and external decoder cross-check. No RF operations."""
import argparse
import ctypes as c
import hashlib
import json
import math
from pathlib import Path
import random
import subprocess
import tempfile
import wave
ROOT = Path(__file__).resolve().parents[2]
REF = "45fc9095fbde8bf6a6988c7dc0de64a4ec0ffe12"
U8 = c.c_uint8
U16 = c.c_uint16
I16 = c.c_int16
class PCM(c.Structure):
    _fields_ = [("tones", U8*26), ("sampleIndex", U16), ("phase", U8)]
def buf(data):
    return (U8*len(data)).from_buffer_copy(data)
def main():
    p = argparse.ArgumentParser()
    p.add_argument("--reference", type=Path)
    p.add_argument("--output", type=Path)
    args = p.parse_args()
    report = []
    with tempfile.TemporaryDirectory() as td:
        td = Path(td)
        so = td/"codec.so"
        subprocess.run(["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic", "-shared", "-fPIC",
                        str(ROOT/"tools/mdc1200/mdc1200_codec.c"), "-o", str(so)], check=True)
        lib = c.CDLL(str(so))
        lib.mdcCRCState.argtypes = [c.POINTER(U8), c.c_size_t]
        lib.mdcCRCState.restype = U16
        for name in ("mdcFEC", "mdcInterleave", "mdcToneBits"):
            getattr(lib, name).argtypes = [c.POINTER(U8), c.c_size_t, c.POINTER(U8), c.c_size_t]
        lib.mdcEncodePostID.argtypes = [U16, c.POINTER(U8), c.c_size_t]
        lib.mdcPCMInit.argtypes = [c.POINTER(PCM), c.POINTER(U8), c.c_size_t]
        lib.mdcPCMRead.argtypes = [c.POINTER(PCM), c.POINTER(I16), c.c_size_t]
        lib.mdcPCMRead.restype = c.c_size_t
        ref = None
        if args.reference:
            refpath = args.reference.resolve()
            assert subprocess.check_output(["git", "-C", str(refpath), "rev-parse", "HEAD"], text=True).strip() == REF
            assert not subprocess.check_output(["git", "-C", str(refpath), "status", "--porcelain"], text=True).strip()
            adapter = td/"reference_adapter.c"
            adapter.write_text(r'''
#include <stdlib.h>
#include "mdc_decode.c"
int reference_decode(short *samples, int count, int *out) {
 mdc_decoder_t *d=mdc_decoder_new(48000);
 if(!d)return -1;
 int result=mdc_decoder_process_samples(d,samples,count);
 unsigned char op=255,arg=255; unsigned short id=0;
 if(result==1 && mdc_decoder_get_packet(d,&op,&arg,&id)==0) {
  out[0]=op;out[1]=arg;out[2]=id;
 }
 free(d);return result;
}
unsigned short reference_crc(unsigned char *data,int count) {return _docrc(data,count);}
''')
            so_ref = td/"reference.so"
            subprocess.run(["gcc", "-shared", "-fPIC", "-I"+str(refpath), str(adapter), "-o", str(so_ref), "-lm"], check=True)
            ref = c.CDLL(str(so_ref))
            ref.reference_decode.argtypes = [c.POINTER(I16), c.c_int, c.POINTER(c.c_int)]
            ref.reference_crc.argtypes = [c.POINTER(U8), c.c_int]
            ref.reference_crc.restype = U16
        def render(frame, chunk=160):
            state = PCM()
            assert lib.mdcPCMInit(c.byref(state), buf(frame), len(frame)) == 0
            pcm = []
            while True:
                block = (I16*chunk)()
                n = lib.mdcPCMRead(c.byref(state), block, chunk)
                pcm.extend(block[:n])
                if n == 0: break
            assert len(pcm) == 8320
            return pcm
        def decode(pcm):
            # Real demodulator input, with 50ms of silence on either side.
            audio = [0]*2400 + pcm + [0]*2400
            out = (c.c_int*3)()
            result = ref.reference_decode((I16*len(audio))(*audio), len(audio), out)
            return result, list(out)
        golden = json.loads((ROOT/"tests/feature_migration/mdc_golden.json").read_text())
        for v in golden["vectors"]:
            unit = int(v["unit_id"],16)
            expected = bytes.fromhex(v["frame"])
            frame = (U8*26)()
            assert lib.mdcEncodePostID(unit, frame,26) == 26
            assert bytes(frame) == expected, "frame construction/interleaving"
            information = bytes.fromhex(v["fec_block"])[:7]
            for n in range(1,5):
                assert lib.mdcCRCState(buf(information), n) == int(v["crc_states_before_xor"][n-1],16)
            assert (lib.mdcCRCState(buf(information),4)^0xffff)==int(v["crc"],16)
            block = (U8*14)()
            assert lib.mdcFEC(buf(information),7,block,14)==14
            assert bytes(block).hex().upper()==v["fec_block"], "FEC parity"
            wire = (U8*14)()
            assert lib.mdcInterleave(block,14,wire,14)==14
            assert bytes(wire)==expected[12:], "MSB wire serialization"
            tones = (U8*26)()
            assert lib.mdcToneBits(frame,26,tones,26)==26
            assert bytes(tones).hex().upper()==v["tone_bits"], "reference differential symbol decisions"
            pcm = render(expected)
            assert pcm == render(expected,1) == render(expected,257), "block boundaries change waveform"
            # Independent analytic waveform: 80-point sine, no phase reset per symbol.
            phase = 0
            for n, sample in enumerate(pcm):
                assert abs(sample-round(12000*math.sin(2*math.pi*phase/80)))<=1
                sym = n//40
                high = (tones[sym//8]>>(7-sym%8))&1
                phase=(phase+(3 if high else 2))%80
            record = dict(unit_id=v["unit_id"], samples=len(pcm), duration_ms=len(pcm)/48,
                          peak=max(map(abs,pcm)), decoder="Not Run")
            if ref:
                result, event = decode(pcm)
                assert result==1 and event==[1,0,unit], (unit,result,event)
                record.update(decoder="Pass", decoded_op=event[0], decoded_arg=event[1], decoded_id=f"{event[2]:04X}")
                # Systematic+FEC information corrupted together, CRC unchanged:
                # decoder must reject, rather than silently correcting back to valid data.
                bad_info = bytearray(information); bad_info[2]^=0x10
                bad_block=(U8*14)();bad_wire=(U8*14)()
                lib.mdcFEC(buf(bad_info),7,bad_block,14)
                lib.mdcInterleave(bad_block,14,bad_wire,14)
                bad_frame=expected[:12]+bytes(bad_wire)
                assert decode(render(bad_frame))[0]==0, "external CRC accepted corrupted ID"
                # A single flipped on-air payload bit is recoverable by external FEC.
                one_bit=bytearray(expected);one_bit[13]^=1
                assert decode(render(one_bit))==(1,[1,0,unit]), "external FEC correction"
                record.update(corrupted_crc_rejected=True,single_bit_fec_recovered=True)
            if args.output:
                args.output.mkdir(parents=True,exist_ok=True)
                path=args.output/(v["unit_id"]+".wav")
                import struct
                with wave.open(str(path),"wb") as wav:
                    wav.setnchannels(1);wav.setsampwidth(2);wav.setframerate(48000)
                    wav.writeframes(struct.pack("<"+"h"*len(pcm),*pcm))
                record["wav_sha256"]=hashlib.sha256(path.read_bytes()).hexdigest()
            report.append(record)
        # Empty/null/short/oversized inputs must not write caller's buffer.
        guard=(U8*40)(*([0xa5]*40)); before=bytes(guard)
        for unit,cap in [(0,26),(1,0),(1,25)]:assert lib.mdcEncodePostID(unit,guard,cap)==-1
        assert lib.mdcEncodePostID(1,None,26)==-1
        for name,valid in [("mdcFEC",7),("mdcInterleave",14),("mdcToneBits",26)]:
            fn=getattr(lib,name)
            for length in (0,valid-1,valid+1):assert fn(guard,length,guard,40)==-1
            assert fn(guard,valid,guard,0)==-1
            assert fn(None,valid,guard,40)==-1
            assert fn(guard,valid,None,40)==-1
        assert bytes(guard)==before
        state=PCM()
        assert lib.mdcPCMInit(None,guard,26)==-1
        assert lib.mdcPCMInit(c.byref(state),guard,25)==-1
        assert lib.mdcPCMRead(None,None,10)==0
        # CRC oracle across independent random payloads, beyond the three goldens.
        if ref:
            rng=random.Random(0x1200)
            for _ in range(1000):
                data=buf(rng.randbytes(4))
                assert (lib.mdcCRCState(data,4)^0xffff)==ref.reference_crc(data,4)
        print(json.dumps(report,indent=2))
        print("PASS: golden CRC/FEC/interleaver/frame/differential/PCM; buffer guards")
        if ref:print("PASS: independent Kaufman PCM demodulation, CRC rejection, FEC correction; 1000 CRC comparisons")
        else:print("NOT RUN: external decoder (pass --reference PATH)")
        if args.output:(args.output/"results.json").write_text(json.dumps(report,indent=2)+"\n")
if __name__=="__main__":main()
