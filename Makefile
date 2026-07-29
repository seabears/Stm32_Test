################################################################################
# Makefile for NUCLEO-F401RE
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

TOOLCHAIN_BIN ?= C:/ST/STM32CubeIDE_2.2.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740/tools/bin
CC := $(TOOLCHAIN_BIN)/arm-none-eabi-gcc
SIZE := $(TOOLCHAIN_BIN)/arm-none-eabi-size
OBJDUMP := $(TOOLCHAIN_BIN)/arm-none-eabi-objdump
OBJCOPY := $(TOOLCHAIN_BIN)/arm-none-eabi-objcopy

OPENOCD ?= C:/ST/STM32CubeIDE_2.2.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.openocd.win32_2.4.500.202604080855/tools/bin/openocd.exe
OPENOCD_SCRIPTS ?= C:/ST/STM32CubeIDE_2.2.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.debug.openocd_2.3.400.202606220929/resources/openocd/st_scripts
OPENOCD_INTERFACE ?= interface/stlink-dap.cfg
OPENOCD_TARGET ?= target/stm32f4x.cfg

RM := rm -rf
MKDIR := mkdir -p

BUILD_DIR := Debug
TARGET := nucleo_f401re
LINKER_SCRIPT := STM32F401RETX_FLASH.ld

C_SRCS := \
Core/Src/main.c \
Core/Src/stm32f4xx_hal_msp.c \
Core/Src/stm32f4xx_it.c \
Core/Src/syscalls.c \
Core/Src/sysmem.c \
Core/Src/system_stm32f4xx.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ramfunc.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c \
Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c

ASM_SRCS := \
Core/Startup/startup_stm32f401retx.s

C_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SRCS))
ASM_OBJS := $(patsubst %.s,$(BUILD_DIR)/%.o,$(ASM_SRCS))
OBJS := $(C_OBJS) $(ASM_OBJS)
DEPS := $(OBJS:.o=.d)

ELF := $(BUILD_DIR)/$(TARGET).elf
BIN := $(BUILD_DIR)/$(TARGET).bin
MAP := $(BUILD_DIR)/$(TARGET).map
LIST := $(BUILD_DIR)/$(TARGET).list
OBJECTS_LIST := $(BUILD_DIR)/objects.list

CPU_FLAGS := -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb
DEFINES := -DDEBUG -DUSE_HAL_DRIVER -DSTM32F401xE
INCLUDES := \
-ICore/Inc \
-IDrivers/STM32F4xx_HAL_Driver/Inc/Legacy \
-IDrivers/STM32F4xx_HAL_Driver/Inc \
-IDrivers/CMSIS/Device/ST/STM32F4xx/Include \
-IDrivers/CMSIS/Include

CFLAGS := $(CPU_FLAGS) -std=gnu11 -g3 $(DEFINES) -c $(INCLUDES) -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage --specs=nano.specs
ASFLAGS := $(CPU_FLAGS) -g3 -DDEBUG -c -x assembler-with-cpp --specs=nano.specs
LDFLAGS := $(CPU_FLAGS) -T"$(LINKER_SCRIPT)" --specs=nosys.specs -Wl,-Map="$(MAP)" -Wl,--gc-sections -static --specs=nano.specs -Wl,--start-group -lc -lm -Wl,--end-group

.PHONY: all main-build clean secondary-outputs flash reset halt

all: main-build

main-build: $(ELF) secondary-outputs
	$(SIZE) $(ELF)

$(ELF) $(MAP): $(OBJS) $(OBJECTS_LIST) $(LINKER_SCRIPT) Makefile
	$(CC) -o "$(ELF)" @"$(OBJECTS_LIST)" $(LDFLAGS)
	@echo 'Finished building target: $(notdir $(ELF))'
	@echo ' '

$(OBJECTS_LIST): $(OBJS)
	@$(MKDIR) "$(dir $@)"
	@printf '"%s"\n' $(OBJS) > "$@"

$(BUILD_DIR)/%.o: %.c Makefile
	@$(MKDIR) "$(dir $@)"
	$(CC) "$<" $(CFLAGS) -MMD -MP -MF"$(@:.o=.d)" -MT"$@" -o "$@"

$(BUILD_DIR)/%.o: %.s Makefile
	@$(MKDIR) "$(dir $@)"
	$(CC) $(ASFLAGS) -MMD -MP -MF"$(@:.o=.d)" -MT"$@" -o "$@" "$<"

$(LIST): $(ELF) Makefile
	$(OBJDUMP) -h -S $(ELF) > "$(LIST)"
	@echo 'Finished building: $(notdir $@)'
	@echo ' '

$(BIN): $(ELF) Makefile
	$(OBJCOPY) -O binary "$(ELF)" "$(BIN)"
	@echo 'Finished building: $(notdir $@)'
	@echo ' '

secondary-outputs: $(LIST) $(BIN)

flash: $(ELF)
	$(OPENOCD) -s "$(OPENOCD_SCRIPTS)" -f $(OPENOCD_INTERFACE) -f $(OPENOCD_TARGET) -c "program $(ELF) verify reset exit"

reset:
	$(OPENOCD) -s "$(OPENOCD_SCRIPTS)" -f $(OPENOCD_INTERFACE) -f $(OPENOCD_TARGET) -c "init; reset run; shutdown"

halt:
	$(OPENOCD) -s "$(OPENOCD_SCRIPTS)" -f $(OPENOCD_INTERFACE) -f $(OPENOCD_TARGET) -c "init; reset halt; shutdown"

clean:
	-$(RM) "$(BUILD_DIR)"
	-@echo ' '

-include $(DEPS)
