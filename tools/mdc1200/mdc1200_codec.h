/* SPDX-License-Identifier: GPL-2.0-only
 * Host experiment, separate from the firmware. See README.md in this directory.
 */
#ifndef MDC1200_CODEC_H
#define MDC1200_CODEC_H
#include <stddef.h>
#include <stdint.h>
#define MDC_FRAME_BYTES 26U
#define MDC_SYMBOLS (MDC_FRAME_BYTES * 8U)
#define MDC_SAMPLE_RATE 48000U
#define MDC_SAMPLES_PER_SYMBOL 40U
#define MDC_PCM_SAMPLES (MDC_SYMBOLS * MDC_SAMPLES_PER_SYMBOL)
uint16_t mdcCRCState(const uint8_t *bytes, size_t length);
int mdcFEC(const uint8_t *information, size_t length, uint8_t *block, size_t capacity);
int mdcInterleave(const uint8_t *block, size_t length, uint8_t *wire, size_t capacity);
int mdcEncodePostID(uint16_t id, uint8_t *wire, size_t capacity);
/* Packed MSB first: 1 selects 1800 Hz, 0 selects 1200 Hz. */
int mdcToneBits(const uint8_t *wire, size_t length, uint8_t *tones, size_t capacity);
typedef struct {
 uint8_t tones[MDC_FRAME_BYTES];
 uint16_t sampleIndex;
 uint8_t phase;
} mdcPCM_t;
int mdcPCMInit(mdcPCM_t *state, const uint8_t *wire, size_t length);
size_t mdcPCMRead(mdcPCM_t *state, int16_t *samples, size_t capacity);
#endif
