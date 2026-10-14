# STM32F103C8T6 Mini 기본 빌드 환경

새 기능 개발의 시작점: `Build`.
기존 프로젝트의 HAL/CMSIS와 GCC 시작 코드를 사용한다.

- Cortex-M3, Flash 64 KB, RAM 20 KB.
- Flash 시작 `0x08000000`, 벡터 테이블 오프셋 0.
- 내부 HSI 8 MHz, HAL 1 ms SysTick, 빈 main 루프.
- USB, DFU 부트로더, FreeRTOS, PWM, 테스트 모듈 없음.
- GPIO 출력 초기화 없음. SWD 디버깅 유지.

## 빌드

STM32CubeIDE 1.15.0에 포함된 GNU Arm GCC 12.3과 GNU make 사용:

```powershell
cd D:\01_GIT\Stm32_Test
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\build.ps1
```

다른 설치 경로는 `-CubeIdeRoot "C:/ST/.../STM32CubeIDE"`로 지정한다.
VS Code에서는 Ctrl+Shift+B로 같은 스크립트를 실행한다.
GNU make와 셸 도구가 PATH에 있으면 `make -j4 all`도 가능하다.
컴파일러 경로 변경: `make TOOLCHAIN_BIN=...`.
산출물: `Debug/stm32f103c8t6.elf`, `.bin`, `.map`, `.list`.

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\build.ps1 -Target clean
powershell -ExecutionPolicy Bypass -File .\Tools\build.ps1 -Target flash
```

`flash`는 ST-LINK/SWD 및 Makefile의 OpenOCD 경로를 사용한다.
전체 Flash 시작부터 기록하므로 기존 부트로더가 있으면 덮어쓴다.
보드 USB 포트의 DFU 다운로드 설정은 포함하지 않는다.
다른 CubeIDE 버전에서 flash/reset/halt를 사용할 때는 Makefile의
OPENOCD와 OPENOCD_SCRIPTS 경로도 설치 경로에 맞게 지정한다.

## 기능 추가

이 브랜치에서 `git switch -c codex/새기능`으로 기능 브랜치를 만든다.
사용자 코드는 `Core/Src/main.c`, HAL 모듈은
`Core/Inc/stm32f1xx_hal_conf.h`, 빌드 소스 목록은 `Makefile`에서 수정한다.
클럭 변경 시 클럭 초기화를 추가하고 HAL 시간 기준도 함께 갱신한다.
이전 기능을 재생성하는 CubeIDE 메타데이터와 .ioc는 제외했다.
이 환경의 빌드 경로는 위의 Makefile/PowerShell이다.
