#
# Minimal USB DFU Bootloader 빌드 파일.
#
# 이 Makefile은 Bootloader_DFU_Min 폴더 안에서 실행하는 것을 기준으로 한다.
# 결과물은 Build/min_dfu_bootloader.elf 와 Build/min_dfu_bootloader.bin 이다.
# flash 타깃은 ST-Link/OpenOCD로 부트로더를 0x08000000에 직접 기록한다.
#

# 상위 앱 프로젝트의 CMSIS 헤더를 같이 사용한다.
PROJECT_ROOT := ..

# STM32CubeIDE가 설치한 ARM GCC toolchain 경로.
TOOLCHAIN_BIN ?= C:/ST/STM32CubeIDE_1.15.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.12.3.rel1.win32_1.0.100.202403111256/tools/bin
CC := $(TOOLCHAIN_BIN)/arm-none-eabi-gcc
SIZE := $(TOOLCHAIN_BIN)/arm-none-eabi-size
OBJDUMP := $(TOOLCHAIN_BIN)/arm-none-eabi-objdump
OBJCOPY := $(TOOLCHAIN_BIN)/arm-none-eabi-objcopy

# ST-Link 플래시에 사용하는 OpenOCD와 STM32CubeIDE 내장 스크립트 경로.
OPENOCD ?= C:/ST/STM32CubeIDE_1.15.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.openocd.win32_2.3.100.202312181736/tools/bin/openocd.exe
OPENOCD_SCRIPTS ?= C:/ST/STM32CubeIDE_1.15.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.debug.openocd_2.2.0.202401261111/resources/openocd/st_scripts
OPENOCD_INTERFACE ?= interface/stlink-dap.cfg
OPENOCD_TARGET ?= target/stm32f1x.cfg

# 빌드 산출물 이름과 링커 스크립트.
BUILD_DIR := Build
TARGET := min_dfu_bootloader
LINKER_SCRIPT := STM32F103C8TX_FLASH_MIN_DFU.ld

# 이 최소 부트로더는 HAL/USB middleware 없이 main.c와 startup만 빌드한다.
C_SRCS := Core/Src/main.c
ASM_SRCS := Core/Startup/startup_stm32f103c8tx.s

# 소스 경로를 그대로 Build 아래 object 경로로 변환한다.
C_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SRCS))
ASM_OBJS := $(patsubst %.s,$(BUILD_DIR)/%.o,$(ASM_SRCS))
OBJS := $(C_OBJS) $(ASM_OBJS)
DEPS := $(OBJS:.o=.d)

# 최종 산출물 경로.
ELF := $(BUILD_DIR)/$(TARGET).elf
BIN := $(BUILD_DIR)/$(TARGET).bin
MAP := $(BUILD_DIR)/$(TARGET).map
LIST := $(BUILD_DIR)/$(TARGET).list
OBJECTS_LIST := $(BUILD_DIR)/objects.list

# STM32F103C8T6 = Cortex-M3, 하드웨어 FPU 없음.
CPU_FLAGS := -mcpu=cortex-m3 -mfloat-abi=soft -mthumb
DEFINES := -DSTM32F103xB
INCLUDES := -ICore/Inc -I$(PROJECT_ROOT)/Drivers/CMSIS/Device/ST/STM32F1xx/Include -I$(PROJECT_ROOT)/Drivers/CMSIS/Include

# -Os: 8 KiB 부트로더 영역에 맞추기 위해 크기 최적화.
# -ffunction-sections/-fdata-sections + --gc-sections: 쓰지 않는 코드/데이터 제거.
# -fno-builtin/-nostartfiles: C 런타임 의존을 줄이고 startup을 직접 사용한다.
CFLAGS := $(CPU_FLAGS) -std=gnu11 -g3 $(DEFINES) -c $(INCLUDES) -Os -ffunction-sections -fdata-sections -Wall -Wextra -fno-builtin -fno-common
ASFLAGS := $(CPU_FLAGS) -g3 -c -x assembler-with-cpp
LDFLAGS := $(CPU_FLAGS) -T"$(LINKER_SCRIPT)" -Wl,-Map="$(MAP)" -Wl,--gc-sections -nostartfiles -Wl,--start-group -lgcc -Wl,--end-group

.PHONY: all clean flash size list

# 기본 빌드: ELF 생성 후 크기 출력, list 생성, bin 변환까지 수행한다.
all: $(ELF) size list $(BIN)

# 모든 object와 링커 스크립트가 준비되면 ELF를 링크한다.
$(ELF) $(MAP): $(OBJS) $(OBJECTS_LIST) $(LINKER_SCRIPT) Makefile
	$(CC) -o "$(ELF)" @"$(OBJECTS_LIST)" $(LDFLAGS)

# Windows command line 길이 문제를 피하려고 object 목록을 파일로 넘긴다.
$(OBJECTS_LIST): $(OBJS)
	@mkdir -p "$(dir $@)"
	@printf '"%s"\n' $(OBJS) > "$@"

# C 소스 하나를 object로 컴파일하고 dependency 파일도 같이 만든다.
$(BUILD_DIR)/%.o: %.c Makefile
	@mkdir -p "$(dir $@)"
	$(CC) "$<" $(CFLAGS) -MMD -MP -MF"$(@:.o=.d)" -MT"$@" -o "$@"

# startup assembly도 같은 방식으로 object로 만든다.
$(BUILD_DIR)/%.o: %.s Makefile
	@mkdir -p "$(dir $@)"
	$(CC) $(ASFLAGS) -MMD -MP -MF"$(@:.o=.d)" -MT"$@" -o "$@" "$<"

# text/data/bss 크기를 확인해 8 KiB 플래시 제한 안에 들어오는지 본다.
size: $(ELF)
	$(SIZE) $(ELF)

# objdump 결과를 남겨 실제 섹션 배치와 디스어셈블리를 확인할 수 있게 한다.
list: $(ELF)
	$(OBJDUMP) -h -S $(ELF) > "$(LIST)"

# dfu-util이나 바이너리 비교에 쓸 raw binary를 만든다.
$(BIN): $(ELF)
	$(OBJCOPY) -O binary "$(ELF)" "$(BIN)"

# ST-Link로 부트로더를 플래시 시작 주소에 기록하고 검증 후 reset한다.
flash: $(ELF)
	$(OPENOCD) -s "$(OPENOCD_SCRIPTS)" -f $(OPENOCD_INTERFACE) -f $(OPENOCD_TARGET) -c "program $(ELF) verify reset exit"

# 빌드 산출물을 제거한다.
clean:
	-rm -rf "$(BUILD_DIR)"

# 컴파일러가 만든 dependency 파일을 포함해 헤더 변경 시 재빌드되게 한다.
-include $(DEPS)
