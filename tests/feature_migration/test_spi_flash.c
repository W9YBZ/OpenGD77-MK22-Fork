/* Exercise the real SPI transport against an edge/timing JEDEC device model. */
#include <assert.h>
#include <stdio.h>
#include "hardware/SPI_Flash.h"
#include "interfaces/gpio.h"
static void pinWrite(GPIO_Type *, uint32_t, uint8_t);
static uint8_t pinRead(GPIO_Type *, uint32_t);
static void pinInit(GPIO_Type *, uint32_t, const gpio_pin_config_t *);
gpio_pin_config_t pin_config_input = {kGPIO_DigitalInput, 0};
gpio_pin_config_t pin_config_output = {kGPIO_DigitalOutput, 0};
static unsigned cycles;
#define SPI_FLASH_PIN_WRITE(port, pin, high) pinWrite(port, pin, high)
#define SPI_FLASH_PIN_READ(port, pin) pinRead(port, pin)
#define SPI_FLASH_DELAY() (cycles += 16)
#define GPIO_PinInit pinInit
#define GPIO_PinWrite pinWrite
#include "../../firmware/source/hardware/SPI_Flash.c"

static bool selected, clockHigh, mosi;
static unsigned bits, command, readyAt, changedAt, rises, transactions;
static int depth;
static uint32_t jedec;
void vPortEnterCritical(void) { assert(depth == 0); depth++; }
void vPortExitCritical(void) { assert(depth == 1); depth--; }
void gpioInitFlash(void) { selected = clockHigh = false; }
static void pinInit(GPIO_Type *port, uint32_t pin, const gpio_pin_config_t *config)
{
    if (config->pinDirection == kGPIO_DigitalOutput)
        pinWrite(port, pin, config->outputLogic);
}
static void pinWrite(GPIO_Type *port, uint32_t pin, uint8_t high)
{
    if (port == GPIO_SPI_FLASH_CS_U && pin == Pin_SPI_FLASH_CS_U)
    {
        if (!high && !selected)
        {
            assert(!clockHigh); selected = true; bits = command = 0;
            readyAt = changedAt = cycles; transactions++;
        }
        if (high && selected)
        {
            assert(!clockHigh && bits == 32 && command == 0x9f);
            assert(cycles >= changedAt + 16); selected = false;
        }
    }
    else if (port == GPIO_SPI_FLASH_DO_U && pin == Pin_SPI_FLASH_DO_U)
    {
        assert(!clockHigh); mosi = high; changedAt = cycles;
    }
    else if (port == GPIO_SPI_FLASH_CLK_U && pin == Pin_SPI_FLASH_CLK_U)
    {
        if (selected && high != clockHigh)
        {
            if (high)
            {
                assert(cycles >= changedAt + 16); // setup/low interval
                if (bits < 8) command = (command << 1) | mosi;
                bits++; rises++;
            }
            else readyAt = cycles + 8; // output valid after falling edge
            changedAt = cycles;
        }
        clockHigh = high;
    }
    else assert(0);
}
static uint8_t pinRead(GPIO_Type *port, uint32_t pin)
{
    assert(port == GPIO_SPI_FLASH_DI_U && pin == Pin_SPI_FLASH_DI_U);
    assert(selected && clockHigh && bits > 0 && bits <= 32);
    assert(cycles >= readyAt && cycles >= changedAt + 16);
    return bits <= 8 ? 0 : (jedec >> (32 - bits)) & 1;
}
int main(void)
{
    const uint16_t ids[] = {0x4014,0x4015,0x4017,0x4018,0x7018,0x0000,0xffff,0x1234};
    for (unsigned i=0;i<sizeof(ids)/sizeof(ids[0]);i++)
    {
        jedec = 0xef0000U | ids[i];
        assert(SPI_Flash_init() == (i < 5));
        assert(flashChipPartNumber == ids[i]);
        assert(!selected && !clockHigh && !depth);
    }
    jedec=0xef4014;
    assert(SPI_Flash_readManufacturer() == 0xef);
    assert(transactions == 9 && rises == 9*32);
    puts("PASS: real SPI JEDEC transport, mode-0 edges/setup/hold/delayed MISO, supported/rejected IDs, read-only commands");
}
