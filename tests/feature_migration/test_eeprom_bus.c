/* Bus-claim race injection into actual EEPROM wrappers; fake bus only. */
#include <assert.h>
#include <stdio.h>
#include "../../firmware/source/hardware/EEPROM.c"
volatile int isI2cInUse;
static int depth,calls;
static bool inject;
void vPortEnterCritical(void) {depth++;if(inject)isI2cInUse=7;}
void vPortExitCritical(void) {assert(depth>0);depth--;}
uint32_t ticksGetMillis(void) {static uint32_t tick;return tick++;}
status_t I2C_MasterTransferBlocking(I2C_Type *base,i2c_master_transfer_t *xfer)
{(void)base;(void)xfer;assert(isI2cInUse==2 && depth==1);calls++;return kStatus_Success;}
int main(void)
{
 uint8_t data=0;
 inject=true;assert(!EEPROM_Read(0,&data,1));assert(!calls&&!depth&&isI2cInUse==7);
 isI2cInUse=0;assert(!EEPROM_Write(0,&data,1));assert(!calls&&!depth&&isI2cInUse==7);
 inject=false;isI2cInUse=0;
 assert(EEPROM_Read(0,&data,1)&&calls==2&&!depth&&!isI2cInUse);
 assert(EEPROM_Write(0,&data,1)&&calls==4&&!depth&&!isI2cInUse);
 puts("PASS: EEPROM claim race rejects MDC ownership without a bus transaction; normal read/write unchanged");
}
