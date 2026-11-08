#include <assert.h>
#include <stdio.h>
#include "functions/fmFeatures.h"
int main(void)
{
 fmFeatures_t d = fmFeaturesDecode(0, 0, 0x12340000);
 assert(d.unitId == 1 && d.mode == PTT_TONE_OFF && d.talkPermit);
 d = fmFeaturesDecode(0, 0, 0xABCD1234);
 assert(d.unitId == 0x1234);
 for (unsigned id = 1; id <= 65535; id++) {
  for (unsigned mode = 0; mode < 3; mode++) {
   for (unsigned permit = 0; permit < 2; permit++) {
    fmFeatures_t a = { id, mode, permit };
    uint16_t r = fmFeaturesRecord(a);
    fmFeatures_t b = fmFeaturesDecode(id, r, 0x9876);
    assert(a.unitId == b.unitId && a.mode == b.mode && a.talkPermit == b.talkPermit);
   }
  }
 }
 fmFeatures_t a = { 0x1234, PTT_TONE_MDC1200, false };
 uint32_t image = ((uint32_t)fmFeaturesRecord(a) << 16) | a.unitId;
 for (unsigned bit = 0; bit < 32; bit++) {
  uint32_t corrupt = image ^ (1U << bit);
  d = fmFeaturesDecode(corrupt, corrupt >> 16, 0x9876);
  assert(d.unitId == 0x9876 && d.mode == PTT_TONE_OFF && d.talkPermit);
 }
 // ID does not follow a later DMR-ID change. Snapshot/restore is byte-exact.
 d = fmFeaturesDecode(a.unitId, fmFeaturesRecord(a), 0x9999);
 assert(d.unitId == 0x1234);
 puts("PASS: 393210 settings records, 32 single-bit corruptions, defaults and stable ID");
}
