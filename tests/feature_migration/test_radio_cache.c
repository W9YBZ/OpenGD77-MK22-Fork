/* Actual radioWriteReg2byte cache, fake bus. No device operations. */
#include <assert.h>
#include <stdio.h>
#include "../../firmware/source/hardware/AT1846S.c"
volatile int isI2cInUse;
static bool fail;
static int busWrites;
status_t I2C_MasterTransferBlocking(I2C_Type *base, i2c_master_transfer_t *xfer)
{
 (void)base;(void)xfer;busWrites++;
 return fail ? kStatus_Fail : kStatus_Success;
}
int main(void)
{
 assert(radioWriteReg2byte(0x3a,0x40,0)==kStatus_Success);
 assert(busWrites==1);
 assert(radioWriteReg2byte(0x3a,0x40,0)==kStatus_Success);
 assert(busWrites==1); // normal cache still works
 fail=true;
 assert(radioWriteReg2byte(0x3a,0x10,0)==kStatus_Fail);
 assert(busWrites==2 && !registerCache[0x3a].cached[0]);
 fail=false;
 assert(radioWriteReg2byte(0x3a,0x40,0)==kStatus_Success);
 assert(busWrites==3); // old value restore is a REAL bus transaction
 fail=true;
 assert(radioWriteReg2byte(0x35,0x27,0x10)==kStatus_Fail);
 fail=false;
 assert(radioWriteReg2byte(0x35,0x27,0x10)==kStatus_Success);
 assert(busWrites==5); // retry of requested value must also reach bus
 puts("PASS: real AT1846 cache invalidates failed writes and retries old/new values");
}
