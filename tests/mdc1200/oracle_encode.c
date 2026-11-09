/* SPDX-License-Identifier: GPL-2.0-only
 * Test adapter to Matthew Kaufman's separately supplied GPL-2.0-only encoder.
 * The external file is included unchanged; none of the new encoder is linked.
 */
#include <stdint.h>
#include <string.h>
#include "mdc_encode.c"
uint16_t oracle_crc(const uint8_t *p, unsigned n) {return _docrc((mdc_u8_t *)p,(int)n);}
int oracle_encode(uint8_t op,uint8_t arg,uint16_t id,uint8_t *frame,uint16_t *states,uint8_t *tones)
{
    mdc_encoder_t *e=mdc_encoder_new(48000);
    if(!e)return -1;
    if(mdc_encoder_set_packet(e,op,arg,id)!=0){free(e);return -1;}
    if(e->loaded!=26){free(e);return -1;}
    memcpy(frame,e->data,26);
    uint8_t data[4]={op,arg,id>>8,id&255};
    for(unsigned i=0;i<4;i++)states[i]=_docrc(data,(int)i+1)^0xffff;
    memset(tones,0,26);
    int last=-1;mdc_sample_t sample;
    // Observe the pristine modem's tone state, including first-symbol seeding.
    while(mdc_encoder_get_samples(e,&sample,1)>0)
    {
        int symbol=e->bpos*8+e->ipos;
        if(symbol>=208)break;
        if(symbol!=last)
        {
            if(e->xorb)tones[symbol/8]|=1U<<(7-symbol%8);
            last=symbol;
        }
    }
    free(e);return last==207 ? 0 : -1;
}
