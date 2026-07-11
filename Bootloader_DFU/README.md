# Custom STM32F103 USB DFU Bootloader

This is an experimental custom USB DFU bootloader for the STM32F103C8T6 project.

## Memory Map

```text
0x08000000 ~ 0x08003FFF : custom DFU bootloader, 16 KiB
0x08004000 ~            : user application
```

The current application must be rebuilt for `0x08004000` before using this bootloader.
Do not use the existing `0x08002000` app image with this bootloader.

## Build

```powershell
cd D:\01_GIT\Stm32_Test\Bootloader_DFU
make -j12 all
```

Output:

```text
Build/custom_dfu_bootloader.elf
Build/custom_dfu_bootloader.bin
```

Current size:

```text
custom_dfu_bootloader.bin = 11484 bytes
```

## Flash Bootloader With ST-LINK

Warning: this replaces the currently installed Maple/STM32duino bootloader.

```powershell
make flash
```

## Application Requirements

The application must use:

```text
FLASH ORIGIN = 0x08004000
VECT_TAB_OFFSET = 0x00004000U
```

A prepared linker script exists at:

```text
D:\01_GIT\Stm32_Test\STM32F103C8TX_FLASH_CUSTOM_DFU.ld
```

## Upload App With dfu-util

This bootloader uses ST DFU/DfuSe style address selection.
The app upload command should be:

```powershell
dfu-util -d 1EAF:0003 -a 0 -s 0x08004000:leave -D Debug/stm32f103c8t6.bin
```

The existing Maple upload command `-a 2` is for the old STM32duino bootloader and is not the right command for this custom bootloader.
## Size Reduction Notes

The first version used HAL RCC/GPIO/FLASH initialization paths and built to:

```text
text  data  bss   bin
13928 216   4616  14144 bytes
```

The current version keeps the ST USB Device Core, ST DFU class, and HAL PCD USB peripheral driver, but changes clock, GPIO disconnect, app jump cleanup, and flash erase/write to direct register control.

Current result:

```text
text  data  bss   bin
11268 216   4584  11484 bytes
```

Removed from the bootloader build:

```text
stm32f1xx_hal_rcc.c
stm32f1xx_hal_rcc_ex.c
stm32f1xx_hal_gpio.c
stm32f1xx_hal_gpio_ex.c
stm32f1xx_hal_pwr.c
stm32f1xx_hal_dma.c
stm32f1xx_hal_exti.c
stm32f1xx_hal_flash.c
stm32f1xx_hal_flash_ex.c
sysmem.c
```

Still intentionally kept:

```text
stm32f1xx_hal.c          : HAL_Init, HAL_Delay, HAL_GetTick, SysTick tick
stm32f1xx_hal_cortex.c   : NVIC helper used by USB PCD init
stm32f1xx_hal_pcd.c      : STM32 USB device peripheral driver
stm32f1xx_hal_pcd_ex.c   : PMA endpoint buffer configuration
stm32f1xx_ll_usb.c       : low-level USB register driver used by HAL PCD
ST USB Device Core       : USB standard request/device framework
ST USB DFU Class         : DFU protocol state machine
```

Further reduction is possible only by replacing HAL PCD/ST USB Device Core with a smaller custom USB device stack. That is a bigger rewrite than the current direct-register cleanup.
