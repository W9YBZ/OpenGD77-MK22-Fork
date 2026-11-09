#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Host-only external MDC validation. Never builds, accesses or transmits a radio."""
import argparse,array,ctypes as C,hashlib,json,math,random,re,subprocess,tempfile,wave
from pathlib import Path
from reference import Oracles,ROOT,U8,U16,LOCK
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--references',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
class Packet(C.Structure):
    _fields_=[('states',U16*4),('info',U8*7),('fec',U8*14),('frame',U8*26),('tones',U8*26)]
def octets(data):return (U8*len(data)).from_buffer_copy(data)
def crc(lib,data):
    result=U16();assert lib.mdc_crc16(octets(data),len(data),C.byref(result));return result.value
def semantic_audit(refs,info):
    # Parse the hash-verified decoder's actual bit-index declarations and verify
    # its actual flag-method expressions. This is source auditing, not Java RX.
    src=(refs/'sdrtrunk/MDCMessage.java').read_text()
    const=lambda name:int(re.search(r'int '+name+r' = (\d+);',src).group(1))
    assert re.search(r'boolean isANI\(\)\s*\{\s*return mMessage.get\(ANI_FLAG\);',src)
    assert re.search(r'boolean isEOT\(\)\s*\{\s*return !mMessage.get\(BOT_EOT_FLAG\);',src)
    assert 'TODO: add CRC' in src
    bits=[0]*40+[(b>>i)&1 for b in info for i in range(8)]
    return dict(ani=bool(bits[const('ANI_FLAG')]),eot=not bool(bits[const('BOT_EOT_FLAG')]))
def pcm_fixture(tones):
    # Ideal host test signal only. No RF path, symbol scheduler or level claim.
    rate=48000;phase=0.0;samples=[0]*1920
    for n in range(208):
        freq=1800 if tones[n//8]>>(7-n%8)&1 else 1200
        for _ in range(40):
            phase=(phase+2*math.pi*freq/rate)%(2*math.pi)
            samples.append(round(12000*math.sin(phase)))
    samples.extend([0]*3840)
    return samples
with tempfile.TemporaryDirectory() as tmp:
    tmp=Path(tmp);oracles=Oracles(a.references,tmp)
    libpath=tmp/'encoder.so'
    subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-fPIC','-shared',str(ROOT/'tests/mdc1200/codec_probe.c'),'-o',str(libpath)],check=True)
    lib=C.CDLL(str(libpath));lib.mdc_encode_single.argtypes=[U8,U8,U16,C.POINTER(Packet)];lib.mdc_encode_single.restype=C.c_bool
    lib.mdc_encode_post_candidate.argtypes=[U16,C.POINTER(Packet)];lib.mdc_encode_post_candidate.restype=C.c_bool
    lib.mdc_crc16.argtypes=[C.POINTER(U8),C.c_size_t,C.POINTER(U16)];lib.mdc_crc16.restype=C.c_bool
    lib.probe_encode_info.argtypes=[C.POINTER(U8),C.POINTER(U8)]
    def encode(op,arg,unit):
        packet=Packet();assert lib.mdc_encode_single(op,arg,unit,C.byref(packet));return packet
    def block_decode(payload):
        fields=(U16*3)();good=oracles.decode.oracle_decode_block(octets(payload),fields)
        return good,list(fields)
    gold=json.loads((ROOT/'tests/mdc1200/golden_vectors.json').read_text());results=[]
    for vector in gold['vectors']:
        op,arg,unit=[int(vector[k],16) for k in ['op','arg','unit_id']]
        regenerated=oracles.vector(op,arg,unit)
        assert all(vector[k]==v for k,v in regenerated.items()),'golden differs from independently rebuilt reference'
        packet=encode(op,arg,unit)
        for key,field in [('info','info'),('fec','fec'),('logical_frame','frame'),('tone_bits','tones')]:
            assert bytes(getattr(packet,field)).hex().upper()==vector[key],key
        assert [f'{n:04X}' for n in packet.states]==vector['crc_prefix_raw']
        assert crc(lib,bytes(packet.info[:4]))==int(vector['crc'],16)
        assert block_decode(bytes(packet.frame[12:]))==(1,[op,arg,unit])
        if vector['profile']=='candidate_post':
            candidate=Packet();assert lib.mdc_encode_post_candidate(unit,C.byref(candidate))
            assert bytes(candidate.info)==bytes(packet.info) and bytes(candidate.frame)==bytes(packet.frame)
        semantic=semantic_audit(a.references,bytes(packet.info))
        assert semantic==dict(ani=op==1,eot=arg==0)
        result=dict(profile=vector['profile'],unit=vector['unit_id'],block_decode='PASS',semantic_source_audit=semantic)
        if vector['profile']=='candidate_post':
            pcm=pcm_fixture(bytes(packet.tones));fields=(U16*3)()
            samples=(C.c_int16*len(pcm))(*pcm)
            assert oracles.decode.oracle_decode_pcm(samples,len(pcm),fields)==1
            assert list(fields)==[op,arg,unit]
            wavpath=a.out/('candidate-post-'+vector['unit_id']+'-ideal.wav')
            raw=array.array('h',pcm)
            import sys
            if sys.byteorder!='little':raw.byteswap()
            with wave.open(str(wavpath),'wb') as wav:
                wav.setparams((1,2,48000,0,'NONE','not compressed'));wav.writeframes(raw.tobytes())
            result['ideal_pcm_external_demodulator']='PASS';result['wav_sha256']=hashlib.sha256(wavpath.read_bytes()).hexdigest()
        results.append(result)
    rng=random.Random(0x4d4443)
    for _ in range(1000):
        data=bytes(rng.randrange(256) for _ in range(rng.randrange(257)))
        assert crc(lib,data)==oracles.encode.oracle_crc(octets(data),len(data))
    for _ in range(512):
        op=1;arg=rng.randrange(256);unit=rng.randrange(65536)
        packet=encode(op,arg,unit);expected=oracles.vector(op,arg,unit)
        assert bytes(packet.frame).hex().upper()==expected['logical_frame']
        assert bytes(packet.tones).hex().upper()==expected['tone_bits']
        assert block_decode(bytes(packet.frame[12:]))==(1,[op,arg,unit])
    base=encode(1,0,0x1234)
    corrected=0
    for n in range(112):
        bad=bytearray(base.frame[12:]);bad[n//8]^=1<<(7-n%8)
        if block_decode(bad)==(1,[1,0,0x1234]):corrected+=1
    assert corrected==112,'single-bit FEC regression'
    # Re-encode deliberately wrong CRC with valid FEC, rather than merely
    # changing one bit that the decoder is supposed to repair.
    for index in range(6):
        info=bytearray(base.info);info[index]^=1
        payload=(U8*14)();lib.probe_encode_info(octets(info),payload)
        assert block_decode(bytes(payload))[0]==0,'external decoder accepted stale/corrupt CRC'
    for flags in [[],['-O2','-flto']]:
        exe=tmp/'guards'
        subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all',*flags,str(ROOT/'host/mdc1200/mdc1200_encoder.c'),str(ROOT/'tests/mdc1200/test_guards.c'),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
    # This stage must not alter the known working radio source or its link graph.
    subprocess.run(['git','diff','--exit-code','af624f3','--','firmware'],cwd=ROOT,check=True)
    assert not list((ROOT/'firmware').rglob('*mdc*'))
    summary=dict(scope='HOST_ONLY_NO_FIRMWARE_CHANGE',kaufman_commit=LOCK['kaufman_commit'],sdrtrunk_commit=LOCK['sdrtrunk_commit'],golden_layers='PASS_7_CASES',random_crc_comparisons=1000,random_packet_comparisons=512,external_block_decodes='PASS',external_ideal_pcm_demodulates='PASS_3_IDS',single_bit_mutations_corrected=corrected,valid_fec_stale_crc_rejections=6,ubsan_and_host_lto='PASS',firmware_unchanged_from='af624f3',rf_decode='NOT_RUN',commercial_post_id_event='UNVERIFIED',vectors=results)
    summary['compiler_version']=subprocess.check_output(['gcc','-dumpfullversion'],text=True).strip()
    summary['encoder_sha256']=hashlib.sha256((ROOT/'host/mdc1200/mdc1200_encoder.c').read_bytes()).hexdigest()
    summary['goldens_sha256']=hashlib.sha256((ROOT/'tests/mdc1200/golden_vectors.json').read_bytes()).hexdigest()
    summary['source_lock_sha256']=hashlib.sha256((ROOT/'host/mdc1200/sources.json').read_bytes()).hexdigest()
    summary['ideal_pcm_fixture']=dict(sample_rate=48000,samples_per_symbol=40,frame_samples=8320,leading_silence_samples=1920,trailing_silence_samples=3840,amplitude=12000,rf_deviation_claim=False)
    (a.out/'results.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary,indent=2))
