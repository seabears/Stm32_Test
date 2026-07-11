# Minimal USB DFU Bootloader

Experimental STM32F103C8T6 USB DFU bootloader with a hand-written USB EP0 control stack.

This folder intentionally does not link ST USB Device Core, ST DFU Class, HAL PCD, or LL USB.

Memory map target:

```text
0x08000000 ~ 0x08001FFF : minimal DFU bootloader, 8 KiB target
0x08002000 ~            : user application
```

Notes:

- This is experimental and has not been flashed to the board yet.
- It implements only endpoint 0 control transfers.
- It handles basic USB enumeration and DfuSe download commands used by `dfu-util -s 0x08002000:leave -D app.bin`.
- The app address is kept at `0x08004000` while this stack is being brought up, even though the bootloader itself targets 8 KiB.

Build:

```powershell
cd D:\01_GIT\Stm32_Test\Bootloader_DFU_Min
make -j12 all
```
## Current build

```text
text  data  bss   bin
2588  8     2104  2596 bytes
```

This fits inside the 8 KiB bootloader region.

Expected upload command after this bootloader is actually flashed:

```powershell
dfu-util -w -d 1EAF:0003 -a 0 -s 0x08002000 -D Debug/stm32f103c8t6.bin
```

Important: this bootloader is built but not yet hardware-tested. The next validation step is ST-LINK flashing the bootloader, then checking whether Windows sees `1EAF:0003` during reset and whether `dfu-util -l` can enumerate it.
## Hardware test result

Tested on the board with ST-LINK flashing the bootloader first.

Confirmed:

```text
ST-LINK flash/verify of Bootloader_DFU_Min succeeded at SWD 950 kHz
Windows enumerated USB DFU as 1EAF:0003
Full app erase succeeded
Full app download to 0x08002000 succeeded
After reset, the app enumerated as USB CDC COM3, VID_0483 PID_5740
```

Working upload command:

```powershell
dfu-util -w -d 1EAF:0003 -a 0 -s 0x08002000 -D Debug/stm32f103c8t6.bin
```

`-w` is useful because the bootloader waits only during the reset-time DFU window. Start `dfu-util` first, then press reset or reset through ST-LINK.

Current caveat:

```text
-s 0x08002000:leave writes the app successfully, but dfu-util reports a final get_status error after the leave/reset sequence.
```

For now, use the non-`:leave` command and reset the board after upload. The app has been observed coming back as COM3 after reset.
