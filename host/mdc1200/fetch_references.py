#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Fetch hash-pinned host oracles; no compiler, firmware or radio operation."""
import argparse,hashlib,json,urllib.request
from pathlib import Path
HERE=Path(__file__).resolve().parent
LOCK=json.loads((HERE/'sources.json').read_text())
def verify(root):
    for item in LOCK['files']:
        path=Path(root)/item['path']
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest()!=item['sha256']:
            raise ValueError('reference missing or hash mismatch: '+str(path))
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    for item in LOCK['files']:
        path=a.out/item['path']
        if path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest()==item['sha256']:continue
        with urllib.request.urlopen(item['url'],timeout=60) as response:data=response.read()
        if hashlib.sha256(data).hexdigest()!=item['sha256']:raise ValueError('download hash mismatch: '+item['path'])
        path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
    verify(a.out);print('PASS: all 9 external reference files match pinned SHA256')
