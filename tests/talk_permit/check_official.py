#!/usr/bin/env python3
"""Verify this fork against the pinned official archive, allowing only the scoped FM audio features."""
import argparse,hashlib,json,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--source-zip',type=Path,required=True);a=p.parse_args()
assert hashlib.sha256(a.source_zip.read_bytes()).hexdigest()=='4491830bc1a7a9e43f82a6151859e42fbfe17a5a8d96c7cc6b65c11faa32af31'
allowed={'firmware/include/functions/settings.h','firmware/include/functions/sound.h',
 'firmware/source/functions/sound.c','firmware/source/user_interface/menuSoundOptions.c',
 'firmware/source/user_interface/uiTxScreen.c','README.md'}
changed=[];same=0
with zipfile.ZipFile(a.source_zip) as z:
 assert z.testzip() is None
 for info in z.infolist():
  if info.is_dir():continue
  rel=info.filename.split('/',1)[1];old=z.read(info);new=(ROOT/rel).read_bytes()
  if rel.endswith(('/.project','/.cproject')):assert old==new,'official project metadata changed'
  if old.replace(b'\r\n',b'\n')!=new.replace(b'\r\n',b'\n'):
   assert rel in allowed,('unexpected source change',rel);changed.append(rel)
  else:same+=1
assert not (ROOT/'firmware/source/runtime/newlib_heap.c').exists()
assert not (ROOT/'firmware/source/functions/mdc1200RF.c').exists()
print(json.dumps(dict(official_files_unchanged_after_eol_normalization=same,allowed_changes=changed,metadata_byte_exact=True,native_backend_linker_drivers_unchanged=True),indent=2))
