/* SPDX-License-Identifier: GPL-2.0-only
 * Expose private layer helpers for negative fixtures; never used as an oracle.
 */
#include "../../host/mdc1200/mdc1200_encoder.c"
void probe_encode_info(const uint8_t *info,uint8_t *payload)
{
    uint8_t fec[14];fec_block(info,fec);interleave(fec,payload);
}
