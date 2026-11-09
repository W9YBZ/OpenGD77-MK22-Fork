/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../host/mdc1200/mdc1200_encoder.h"
int main(void)
{
    mdc_encoded_t packet,before;memset(&packet,0xa5,sizeof(packet));before=packet;
    assert(!mdc_encode_post_candidate(0,&packet));assert(!memcmp(&packet,&before,sizeof(packet)));
    assert(!mdc_encode_single(1,0,1,NULL));assert(!mdc_encode_post_candidate(1,NULL));
    assert(mdc_encode_single(1,0,0,&packet)); // generic encoder does not silently remap ID
    uint16_t crc=0x1234;
    assert(!mdc_crc16(NULL,1,&crc) && crc==0x1234);
    assert(!mdc_crc16((const uint8_t *)"a",1,NULL));
    assert(mdc_crc16(NULL,0,&crc) && crc==0xffff);
    for(unsigned id=1;id<=0xffff;id++)
    {
        assert(mdc_encode_post_candidate((uint16_t)id,&packet));
        assert(packet.info[0]==1 && packet.info[1]==0);
        assert(packet.info[2]==(id>>8) && packet.info[3]==(id&255));
    }
    uint8_t out[28],original[28];memset(out,0x5a,sizeof(out));memcpy(original,out,sizeof(out));
    for(size_t capacity=0;capacity<MDC_FRAME_BYTES;capacity++)
    {
        assert(!mdc_copy_frame(&packet,out,capacity));assert(!memcmp(out,original,sizeof(out)));
    }
    assert(!mdc_copy_frame(NULL,out,sizeof(out)));assert(!mdc_copy_frame(&packet,NULL,sizeof(out)));
    assert(mdc_copy_frame(&packet,out+1,26)==26);assert(out[0]==0x5a && out[27]==0x5a);
    assert(!memcmp(out+1,packet.frame,26));
    puts("PASS: all 65535 nonzero IDs, raw ID zero, null/empty CRC, rejected output unchanged, capacities 0..25 and exact-boundary canaries");
}
