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
/* Release-only tail controller. UI release latch prevents re-key while active. */
typedef struct { bool attempted, active; uint32_t started; } fmTail_t;
typedef enum { FM_TAIL_NONE, FM_TAIL_START, FM_TAIL_WAIT, FM_TAIL_STOP } fmTailAction_t;
static inline fmTailAction_t fmTailPoll(fmTail_t *tail, bool eligible, bool abort,
                                       uint8_t mode, uint32_t now)
{
 if (tail->active) {
  if (abort || !eligible || (uint32_t)(now - tail->started) >= 150U) {
   tail->active = false;
   return FM_TAIL_STOP;
  }
  return FM_TAIL_WAIT;
 }
 if (tail->attempted) return FM_TAIL_NONE;
 tail->attempted = true;
 // MDC1200 is deliberately RF-disabled even when selected and persisted.
 if (eligible && !abort && mode == PTT_TONE_BEEP) {
  tail->active = true;
  tail->started = now;
  return FM_TAIL_START;
 }
 return FM_TAIL_NONE;
}
#endif
