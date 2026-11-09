/* Test the real settings helpers; never invoke settingsStorage or a radio. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../firmware/source/functions/settings.c"
int main(void)
{
 const uint32_t mask=BIT_FM_PTT_TONE_LOW | BIT_FM_PTT_TONE_HIGH;
 memset(&nonVolatileSettings,0,sizeof(nonVolatileSettings));
 assert(settingsGetPttToneMode()==PTT_TONE_OFF);
 for(unsigned raw=0;raw<4;raw++)
 {
  memset(&nonVolatileSettings,0xa5,sizeof(nonVolatileSettings));
  uint32_t before=nonVolatileSettings.bitfieldOptions[0];
  settingsDirty=false;settingsSetPttToneMode(raw);
  assert(settingsDirty);
  assert(settingsGetPttToneMode()==(raw<=2?raw:0));
  assert((nonVolatileSettings.bitfieldOptions[0]&~mask)==(before&~mask));
  settingsStruct_t expected;memset(&expected,0xa5,sizeof(expected));
  expected.bitfieldOptions[0]=(before&~mask)|((raw<=2?raw:0)<<24);
  assert(!memcmp(&expected,&nonVolatileSettings,sizeof(expected)));
  // The menu's existing cancel snapshot and binary settings representation
  // preserve both the selected mode and every neighboring field.
  settingsStruct_t saved=nonVolatileSettings;
  settingsSetPttToneMode((raw+1)%3);nonVolatileSettings=saved;
  assert(!memcmp(&saved,&nonVolatileSettings,sizeof(saved)));
 }
 nonVolatileSettings.bitfieldOptions[0]|=mask;assert(settingsGetPttToneMode()==PTT_TONE_OFF);
 settingsSetPttToneMode(255);assert(settingsGetPttToneMode()==PTT_TONE_OFF);
 puts("PASS: real settings bit helpers, Off default, three-mode range, invalid->Off, dirty flag, byte/neighbor isolation and snapshot restoration");
}
