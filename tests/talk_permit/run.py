#!/usr/bin/env python3
"""Talk-Permit-only host tests; never builds or flashes a radio image."""
from pathlib import Path
import re,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2]
incs=['-I'+str(d) for d in (ROOT/'firmware').rglob('*') if d.is_dir()]
common=['-std=gnu11','-DSDK_OS_FREE_RTOS','-DCPU_MK22FN512VLL12','-DGITVERSION="host"',*incs]
with tempfile.TemporaryDirectory() as tmp:
 for flags in ([],['-O2','-flto']):
  exe=str(Path(tmp)/'test')
  subprocess.run(['gcc',*common,'-DPLATFORM_GD77','-DHAS_GPS=1','-Wall','-Wextra','-Wno-int-to-pointer-cast','-Wno-pointer-to-int-cast','-ffunction-sections','-fdata-sections','-fsanitize=undefined','-fno-sanitize-recover=all',*flags,str(ROOT/'tests/talk_permit/test_talk_permit.c'),'-Wl,--gc-sections','-o',exe],check=True)
  subprocess.run([exe],check=True)
for platform in ['PLATFORM_GD77','PLATFORM_GD77S','PLATFORM_DM1801','PLATFORM_RD5R']:
 for f in ['functions/sound.c','user_interface/uiTxScreen.c','user_interface/menuSoundOptions.c']:
  subprocess.run(['gcc',*common,'-D'+platform,'-fsyntax-only','-Werror=implicit-function-declaration','-Wno-int-to-pointer-cast','-Wno-pointer-to-int-cast',str(ROOT/'firmware/source'/f)],check=True)
 print('PASS syntax:',platform)
s=(ROOT/'firmware/source/functions/sound.c').read_text()
notes=re.search(r'MELODY_FM_TALK_PERMIT\[\] = \{([^}]+)',s).group(1)
assert [int(n) for n in re.findall(r'-?\d+',notes)]==[940,40,0,10,940,40,0,10,940,40,-1,-1]
assert 'BIT_UNUSED_1' in (ROOT/'firmware/include/functions/settings.h').read_text()
print('PASS: nominal three-tone 140ms melody; setting aliases official unused bank-0 bit 2')

# Keep the menu boundary in the test: initialization deliberately has no PTT.
menu=(ROOT/'firmware/source/user_interface/menuSystem.c').read_text()
init=menu.split('static void menuSystemPushMenuFirstRun(void)',1)[1].split('void menuSystemPushNewMenu',1)[0]
assert '.buttons = 0' in init and 'function(&ev, true)' in init
tx=(ROOT/'firmware/source/user_interface/uiTxScreen.c').read_text()
first,real=tx.split('menuStatus_t menuTxScreen(',1)[1].split('talkPermitTick(ev);',1)
assert 'talkPermitPending = !transmitError;' in first and 'talkPermitStart(ev)' not in first
assert 'if (talkPermitActive && ev->hasEvent)' not in tx
print('PASS: official synthetic first-run boundary arms pending request; real menu tick consumes it')
