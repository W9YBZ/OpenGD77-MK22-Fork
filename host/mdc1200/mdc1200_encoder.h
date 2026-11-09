/* SPDX-License-Identifier: GPL-2.0-only
 * Isolated host experiment. Not part of the firmware or RF transmitter.
 */
#ifndef MDC1200_HOST_ENCODER_H
#define MDC1200_HOST_ENCODER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum { MDC_INFO_BYTES=7, MDC_FEC_BYTES=14, MDC_FRAME_BYTES=26, MDC_SYMBOLS=208 };
typedef struct
{
    uint16_t crc_prefix_raw[4];
    uint8_t info[MDC_INFO_BYTES];
    uint8_t fec[MDC_FEC_BYTES];
    uint8_t frame[MDC_FRAME_BYTES]; // logical bytes, before differential decisions
    uint8_t tones[MDC_FRAME_BYTES]; // packed MSB-first: 1=1800 Hz, 0=1200 Hz
} mdc_encoded_t;
bool mdc_crc16(const uint8_t *data,size_t length,uint16_t *result);
// Generic raw single packet. Zero ID allowed for protocol boundary tests.
bool mdc_encode_single(uint8_t op,uint8_t arg,uint16_t id,mdc_encoded_t *out);
// Candidate ANI/EOT semantics; NOT RF/commercial compatibility certification.
bool mdc_encode_post_candidate(uint16_t id,mdc_encoded_t *out);
// Returns 0 on invalid/short output; never partially writes a rejected buffer.
size_t mdc_copy_frame(const mdc_encoded_t *frame,uint8_t *out,size_t capacity);
#endif
