#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Generate goldens solely from the external, hash-verified original encoder."""
import argparse,json,tempfile
from pathlib import Path
from reference import Oracles,LOCK
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--references',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
with tempfile.TemporaryDirectory() as tmp:
    oracle=Oracles(a.references,tmp)
    vectors=[dict(profile=label,**oracle.vector(op,arg,unit)) for label,op,arg,unit in
        [('candidate_post',1,0,u) for u in [1,0x1234,0xffff]]+
        [('bot_control',1,0x80,u) for u in [1,0x1234,0xffff]]+
        [('conflicting_preset_control',0,0x80,0x1234)]]
    result=dict(oracle='atmatthewat/mdc-encode-decode',commit=LOCK['kaufman_commit'],evidence='external software, not captured RF',vectors=vectors)
    a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(result,indent=2)+'\n')
    print('PASS: generated 7 vectors with pristine external encoder, no new codec linked')
