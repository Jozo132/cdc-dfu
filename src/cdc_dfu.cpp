/*
 * cdc-dfu: Zero-overhead DFU bootloader jump for STM32F4 via USB CDC.
 *
 * Compiled automatically by PlatformIO when listed in lib_deps.
 * No #include needed in user code.
 *
 * Silently compiles to nothing if guards are not met (wrong MCU, no USB CDC, etc.)
 */

#if defined(STM32F4xx) && defined(USBCON) && defined(USBD_USE_CDC) && defined(DTR_TOGGLING_SEQ)

#include <Arduino.h>
#include "stm32f4xx_hal.h"
#include "usbd_cdc_if.h"
#include "usbd_cdc.h"

#define STM32_ROM_BOOTLOADER 0x1FFF0000UL
#define DFU_REQUEST_MAGIC    0x424F4F54UL  // "BOOT"

// Persists across NVIC_SystemReset - not zeroed by C runtime
__attribute__((section(".noinit"))) static volatile uint32_t _dfu_request_flag;

// USB device handle (defined in usbd_cdc_if.c)
extern USBD_HandleTypeDef hUSBD_Device_CDC;

// Magic bytes: host toggles DTR >3 times then sends "DFU!"
#define DFU_MAGIC_LEN 4
static const uint8_t dfu_magic[DFU_MAGIC_LEN] = { 'D', 'F', 'U', '!' };


/*
 * Runs before main() (Thread mode, after SystemInit).
 * If the noinit flag contains our magic, jump to ROM bootloader.
 * Uses raw CMSIS register access only - HAL is not initialized yet.
 */
__attribute__((constructor(101)))
static void _checkDfuRequest()
{
    if (_dfu_request_flag != DFU_REQUEST_MAGIC)
        return;

    _dfu_request_flag = 0;  // Clear so we don't loop

    __disable_irq();

    // Disable SysTick
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    // Disable all NVIC interrupts and clear pending
    for (uint8_t i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    // Reset USB OTG FS peripheral so bootloader can reinitialize it cleanly
    RCC->AHB2ENR  |= RCC_AHB2ENR_OTGFSEN;
    RCC->AHB2RSTR |= RCC_AHB2RSTR_OTGFSRST;
    __DSB();
    RCC->AHB2RSTR &= ~RCC_AHB2RSTR_OTGFSRST;

    // Reset GPIO port A (PA11/PA12 = USB D-/D+) to default analog state
    RCC->AHB1RSTR |= RCC_AHB1RSTR_GPIOARST;
    __DSB();
    RCC->AHB1RSTR &= ~RCC_AHB1RSTR_GPIOARST;

    // Reset clocks back to HSI (SystemInit may have switched to PLL/HSE)
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY)) {}
    RCC->CFGR = 0;  // Switch system clock to HSI
    while ((RCC->CFGR & RCC_CFGR_SWS) != 0) {}
    RCC->CR &= ~(RCC_CR_PLLON | RCC_CR_HSEON | RCC_CR_CSSON);

    // Remap system memory to 0x00000000
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    __DSB();
    SYSCFG->MEMRMP = 0x01;
    __DSB();

    uint32_t bootAddr  = STM32_ROM_BOOTLOADER;
    uint32_t bootStack = *(volatile uint32_t*) bootAddr;
    uint32_t bootEntry = *(volatile uint32_t*)(bootAddr + 4);

    SCB->VTOR = bootAddr;
    __set_MSP(bootStack);

    // Re-enable interrupts - the ROM bootloader needs them for USB enumeration
    __enable_irq();

    ((void (*)(void))bootEntry)();
    while (1);
}


/*
 * DTR toggling hook - called by the framework (DTR_TOGGLING_SEQ) from USB
 * receive interrupt after >3 DTR toggles. Zero overhead when not triggered.
 *
 * Runs in ISR context, so we disconnect USB, set a flag in noinit RAM,
 * and reset. The constructor above catches the flag and jumps to bootloader
 * from Thread mode.
 */
extern "C" void dtr_togglingHook(uint8_t *buf, uint32_t *len)
{
    if (*len >= DFU_MAGIC_LEN &&
        buf[0] == dfu_magic[0] && buf[1] == dfu_magic[1] &&
        buf[2] == dfu_magic[2] && buf[3] == dfu_magic[3])
    {
        // Disconnect USB so host sees device leave
        USBD_Stop(&hUSBD_Device_CDC);
        USBD_DeInit(&hUSBD_Device_CDC);
        USB_OTG_FS->GCCFG &= ~USB_OTG_GCCFG_PWRDWN;

        // Set DFU request flag (survives soft reset)
        _dfu_request_flag = DFU_REQUEST_MAGIC;

        NVIC_SystemReset();
    }
}

#endif // STM32F4xx && USBCON && USBD_USE_CDC && DTR_TOGGLING_SEQ
