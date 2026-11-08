/* GD-77 feature record. New code; distributed under repository license.txt. */
#ifndef FM_FEATURES_H
#define FM_FEATURES_H
#include <stdbool.h>
#include <stdint.h>

typedef enum { PTT_TONE_OFF, PTT_TONE_BEEP, PTT_TONE_MDC1200 } pttToneMode_t;
typedef struct { uint16_t unitId; uint8_t mode; bool talkPermit; } fmFeatures_t;

/* CRC-8/ATM, init 0, no reflection or final xor. Slots never extend settings. */
static inline uint8_t fmFeaturesCRC(uint16_t id, uint8_t flags)
{
 uint8_t bytes[3] = { (uint8_t)id, (uint8_t)(id >> 8), flags }, crc = 0;
 for (unsigned i = 0; i < 3; i++) {
  crc ^= bytes[i];
  for (unsigned b = 0; b < 8; b++) crc = (uint8_t)((crc << 1) ^ ((crc & 0x80) ? 7 : 0));
 }
 return crc;
}
static inline fmFeatures_t fmFeaturesDecode(uint16_t id, uint16_t record, uint32_t dmrId)
{
 uint8_t flags = (uint8_t)record;
 fmFeatures_t f = { id, flags & 3U, (flags & 4U) != 0 };
 if (!id || (flags & 0xF8U) != 0xA8U || f.mode > PTT_TONE_MDC1200 ||
     (record >> 8) != fmFeaturesCRC(id, flags)) {
  f.unitId = (uint16_t)dmrId;
  if (!f.unitId) f.unitId = 1;
  f.mode = PTT_TONE_OFF;
  f.talkPermit = true;
 }
 return f;
}
static inline uint16_t fmFeaturesRecord(fmFeatures_t f)
{
 uint8_t flags = 0xA8U | (f.mode <= PTT_TONE_MDC1200 ? f.mode : PTT_TONE_OFF) | (f.talkPermit ? 4U : 0U);
 return flags | ((uint16_t)fmFeaturesCRC(f.unitId, flags) << 8);
}
#endif
