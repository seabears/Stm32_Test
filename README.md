# NUCLEO-F401RE

NUCLEO-F401RE용 최소 STM32 HAL 프로젝트입니다.

포함된 기능은 다음 두 가지뿐입니다.

- HSI/PLL 기반 84 MHz 시스템 클록
- TIM1 업데이트 인터럽트 기반 1 ms 틱 (`g_tim1_tick_ms`)
- 온보드 LD2(PA5) GPIO 출력 초기화
- ST-LINK 가상 COM 포트용 USART2(PA2/PA3), 115200-8-N-1 및 `printf()` 출력

USB Device, PWM, 추가 사용자 GPIO, 부트로더 코드는 포함하지 않습니다.

## UART 디버깅

보드의 ST-LINK USB를 PC에 연결한 뒤 생성된 COM 포트를 115200 baud, 8 data bits, no parity, 1 stop bit로 엽니다. 부팅하면 다음 메시지가 출력됩니다.

```text
NUCLEO-F401RE UART debug ready
```

애플리케이션에서는 일반 `printf()`를 사용할 수 있습니다.

## 명령줄 빌드

저장소 루트에서 실행합니다.

```powershell
.\build.cmd
```

`build.cmd`는 `C:\ST` 아래에서 설치된 최신 STM32CubeIDE의 GNU Arm 툴체인과 Make를 찾아 사용합니다. Make를 PATH에 등록한 환경에서는 직접 실행할 수도 있습니다.

```powershell
make TOOLCHAIN_BIN=C:/path/to/arm-toolchain/bin -j12 all
```

산출물은 `Debug/` 아래에 생성됩니다.

```powershell
.\build.cmd clean
.\build.cmd flash
```

`make flash`는 NUCLEO 보드의 온보드 ST-LINK와 OpenOCD의 `target/stm32f4x.cfg`를 사용합니다.

## VS Code

- `Ctrl+Shift+B`: `STM32: Build`
- `Terminal > Run Task > STM32: Flash`: 빌드 후 ST-LINK로 program/verify/reset

## STM32CubeIDE

이 폴더를 기존 STM32CubeIDE 프로젝트로 가져올 수 있습니다. 타깃 MCU는 STM32F401RETx이며 링커 스크립트는 `STM32F401RETX_FLASH.ld`입니다.
