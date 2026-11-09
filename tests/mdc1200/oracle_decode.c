/* SPDX-License-Identifier: GPL-2.0-only
 * Unchanged external decoder; this adapter never links the new encoder.
 */
#include <stdint.h>
#include <string.h>
#include "mdc_decode.c"
int oracle_decode_block(const uint8_t *payload,uint16_t *fields)
{
    mdc_decoder_t *d=mdc_decoder_new(48000);if(!d)return -1;
    for(int i=0;i<112;i++)d->du[0].bits[i]=(payload[i/8]>>(7-i%8))&1;
    _procbits(d,0);
    int good=d->good;
    if(good==1){fields[0]=d->op;fields[1]=d->arg;fields[2]=d->unitID;}
    free(d);return good;
}
int oracle_decode_pcm(const int16_t *samples,int count,uint16_t *fields)
{
    mdc_decoder_t *d=mdc_decoder_new(48000);if(!d)return -1;
    int good=mdc_decoder_process_samples(d,(mdc_sample_t *)samples,count);
    if(good==1){fields[0]=d->op;fields[1]=d->arg;fields[2]=d->unitID;}
    free(d);return good;
}
