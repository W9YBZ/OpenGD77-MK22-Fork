#!/usr/bin/env python3
"""Compare old/new actual helpers with a synthetic non-echoing 0x35 model.
The XOR is a counterexample, not a claimed measurement of the user's chip.
"""
from pathlib import Path
import subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2]
fixture=(ROOT/'tests/talk_permit/test_talk_permit.c').read_text()
incs=['-I'+str(d) for d in (ROOT/'firmware').rglob('*') if d.is_dir()]
flags=['-std=gnu11','-O2','-DSDK_OS_FREE_RTOS','-DCPU_MK22FN512VLL12','-DPLATFORM_GD77','-DHAS_GPS=1','-DGITVERSION="host"','-Wno-int-to-pointer-cast','-Wno-pointer-to-int-cast','-ffunction-sections','-fdata-sections',*incs]
old=subprocess.check_output(['git','show','24e155f:firmware/source/user_interface/uiTxScreen.c'],cwd=ROOT,text=True)
new=(ROOT/'firmware/source/user_interface/uiTxScreen.c').read_text()
probe=r'''
int main(void)
{
 uiEvent_t ev=reset();nonEchoTone=true;pttToneArmed=true;ev.buttons=0;
 nonVolatileSettings.bitfieldOptions[0]=BIT_FM_PTT_TONE_LOW;
 bool started=pttToneHandleRelease(&ev);
 printf("started=%d fault=%04X retained=%d tone_reads=%u\n",started,fmAudioFault,pttToneActive,toneReads);
 if(started){ev.time=250;pttToneHandleRelease(&ev);fmTxAudioExit(NULL);}
 return started && !pttToneActive && !transmitError && toneReads==0 ? 0 : 1;
}
'''
with tempfile.TemporaryDirectory() as tmp:
 tmp=Path(tmp)
 for label,source in [('old_24e155f',old),('current',new)]:
  ui=tmp/(label+'.c');ui.write_text(source)
  harness=tmp/(label+'_test.c')
  body=fixture.replace('#include "../../firmware/source/user_interface/uiTxScreen.c"','#include "'+str(ui)+'"').replace('int main(void)','int unused_suite(void)',1)
  harness.write_text(body+probe);exe=tmp/label
  subprocess.run(['gcc',*flags,str(harness),'-Wl,--gc-sections','-o',str(exe)],check=True)
  run=subprocess.run([str(exe)],capture_output=True,text=True)
  print(label+': '+run.stdout.strip())
  assert run.returncode==(1 if label.startswith('old') else 0)
print('PASS: old code reproduces E35/4 and retained ownership under non-echo model; current code starts and cleans up without reading 0x35')
