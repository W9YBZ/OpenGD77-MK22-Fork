/* Exercise the actual official AT1846 register/cache implementation. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../firmware/source/hardware/AT1846S.c"
volatile int isI2cInUse;
static unsigned transfers;
static bool failTransfer;
static uint16_t hardware[128];
status_t I2C_MasterTransferBlocking(I2C_Type *base, i2c_master_transfer_t *xfer)
{
 assert(base==I2C0 && xfer->direction==kI2C_Write && xfer->dataSize==3);
 transfers++;
 if(failTransfer)return kStatus_Fail;
 hardware[xfer->data[0]]=((uint16_t)xfer->data[1]<<8)|xfer->data[2];
 return kStatus_Success;
}
int main(void)
{
 // Reproduce stale cached-success with the real unchanged native API.
 failTransfer=true;assert(radioWriteReg2byte(0x3a,0x40,0xac)!=kStatus_Success);
 assert(transfers==1 && hardware[0x3a]==0 && registerCache[0x3a].cached[0]);
 failTransfer=false;assert(radioWriteReg2byte(0x3a,0x40,0xac)==kStatus_Success);
 assert(transfers==1 && hardware[0x3a]==0); // no physical restore happened
 // The opt-in recovery API must reach I2C and heal the physical state.
 assert(radioWriteReg2byteFresh(0x3a,0x40,0xac)==kStatus_Success);
 assert(transfers==2 && hardware[0x3a]==0x40ac);
 assert(radioWriteReg2byteFresh(0x3a,0x40,0xac)==kStatus_Success);
 assert(transfers==3); // Never elide a recovery write, even if cached equal.
 failTransfer=true;assert(radioWriteReg2byteFresh(0x35,0x27,0x10)!=kStatus_Success);
 assert(!registerCache[0x35].cached[0]);
 failTransfer=false;assert(radioWriteReg2byteFresh(0x35,0x27,0x10)==kStatus_Success);
 assert(hardware[0x35]==10000 && registerCache[0x35].cached[0]);
 unsigned before=transfers;isI2cInUse=4;
 assert(radioWriteReg2byteFresh(0x3a,0,0)!=kStatus_Success && transfers==before);
 isI2cInUse=0;
 assert(radioWriteReg2byteFresh(0x7f,0,1)!=kStatus_Success && currentRegisterBank==0 && transfers==before);
 puts("PASS: actual native failed-write cache bug reproduced; fresh writes reach physical bus, failed transfers invalidate cache, busy/bank writes rejected");
}
