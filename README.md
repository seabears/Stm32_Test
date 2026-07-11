# stm32f103c8t6

STM32F103C8Tx firmware project moved from STM32CubeIDE workspace.

## Build from command line

Run from this repository root:

```powershell
make -j12 all
```

The root `Makefile` builds with the GNU Arm toolchain bundled in STM32CubeIDE 1.15.0:

```text
C:\ST\STM32CubeIDE_1.15.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.12.3.rel1.win32_1.0.100.202403111256\tools\bin
```

Build artifacts are generated under `Debug/` and ignored by Git.

Useful commands:

```powershell
make clean
make -j12 all
make flash
```

`make flash` uses ST-LINK through OpenOCD bundled with STM32CubeIDE. It does not use `stm32cubeidec`. The Makefile uses `interface/stlink-dap.cfg` to avoid the older HLA OpenOCD path.

## Build from STM32CubeIDE

You can still import this folder as an existing STM32CubeIDE project. The CubeIDE metadata files (`.project`, `.cproject`, `.settings/`) are kept in the repository.