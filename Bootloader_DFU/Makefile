################################################################################
# Makefile for custom STM32F103C8T6 USB DFU bootloader
################################################################################

PROJECT_ROOT := ..
TOOLCHAIN_BIN ?= C:/ST/STM32CubeIDE_1.15.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.12.3.rel1.win32_1.0.100.202403111256/tools/bin
CC := $(TOOLCHAIN_BIN)/arm-none-eabi-gcc
SIZE := $(TOOLCHAIN_BIN)/arm-none-eabi-size
OBJDUMP := $(TOOLCHAIN_BIN)/arm-none-eabi-objdump
OBJCOPY := $(TOOLCHAIN_BIN)/arm-none-eabi-objcopy

OPENOCD ?= C:/ST/STM32CubeIDE_1.15.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.openocd.win32_2.3.100.202312181736/tools/bin/openocd.exe
OPENOCD_SCRIPTS ?= C:/ST/STM32CubeIDE_1.15.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.debug.openocd_2.2.0.202401261111/resources/openocd/st_scripts
OPENOCD_INTERFACE ?= interface/stlink-dap.cfg
OPENOCD_TARGET ?= target/stm32f1x.cfg

RM := rm -rf
MKDIR := mkdir -p

BUILD_DIR := Build
TARGET := custom_dfu_bootloader
LINKER_SCRIPT := STM32F103C8TX_FLASH_DFU_BOOT.ld

C_SRCS := \
Core/Src/main.c \
Core/Src/stm32f1xx_it.c \
USB/Src/usbd_conf.c \
USB/Src/usbd_desc.c \
USB/Src/usbd_dfu_if.c \
$(PROJECT_ROOT)/Core/Src/system_stm32f1xx.c \
$(PROJECT_ROOT)/Core/Src/syscalls.c \
$(PROJECT_ROOT)/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c \
$(PROJECT_ROOT)/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c \
$(PROJECT_ROOT)/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pcd.c \
$(PROJECT_ROOT)/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pcd_ex.c \
$(PROJECT_ROOT)/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_usb.c \
$(PROJECT_ROOT)/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_core.c \
$(PROJECT_ROOT)/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ctlreq.c \
$(PROJECT_ROOT)/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ioreq.c \
$(PROJECT_ROOT)/Middlewares/ST/STM32_USB_Device_Library/Class/DFU/Src/usbd_dfu.c

ASM_SRCS := Core/Startup/startup_stm32f103c8tx.s

C_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SRCS))
ASM_OBJS := $(patsubst %.s,$(BUILD_DIR)/%.o,$(ASM_SRCS))
OBJS := $(C_OBJS) $(ASM_OBJS)
DEPS := $(OBJS:.o=.d)

ELF := $(BUILD_DIR)/$(TARGET).elf
BIN := $(BUILD_DIR)/$(TARGET).bin
MAP := $(BUILD_DIR)/$(TARGET).map
LIST := $(BUILD_DIR)/$(TARGET).list
OBJECTS_LIST := $(BUILD_DIR)/objects.list

CPU_FLAGS := -mcpu=cortex-m3 -mfloat-abi=soft -mthumb
DEFINES := -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB
INCLUDES := \
-ICore/Inc \
-IUSB/Inc \
-I$(PROJECT_ROOT)/Middlewares/ST/STM32_USB_Device_Library/Core/Inc \
-I$(PROJECT_ROOT)/Middlewares/ST/STM32_USB_Device_Library/Class/DFU/Inc \
-I$(PROJECT_ROOT)/Drivers/STM32F1xx_HAL_Driver/Inc/Legacy \
-I$(PROJECT_ROOT)/Drivers/STM32F1xx_HAL_Driver/Inc \
-I$(PROJECT_ROOT)/Drivers/CMSIS/Device/ST/STM32F1xx/Include \
-I$(PROJECT_ROOT)/Drivers/CMSIS/Include

CFLAGS := $(CPU_FLAGS) -std=gnu11 -g3 $(DEFINES) -c $(INCLUDES) -Os -ffunction-sections -fdata-sections -Wall --specs=nano.specs
ASFLAGS := $(CPU_FLAGS) -g3 -DDEBUG -c -x assembler-with-cpp --specs=nano.specs
LDFLAGS := $(CPU_FLAGS) -T"$(LINKER_SCRIPT)" --specs=nosys.specs -Wl,-Map="$(MAP)" -Wl,--gc-sections -static --specs=nano.specs -Wl,--start-group -lc -lm -Wl,--end-group

.PHONY: all clean flash size list

all: $(ELF) size list $(BIN)

$(ELF) $(MAP): $(OBJS) $(OBJECTS_LIST) $(LINKER_SCRIPT) Makefile
	$(CC) -o "$(ELF)" @"$(OBJECTS_LIST)" $(LDFLAGS)

$(OBJECTS_LIST): $(OBJS)
	@$(MKDIR) "$(dir $@)"
	@printf '"%s"\n' $(OBJS) > "$@"

$(BUILD_DIR)/%.o: %.c Makefile
	@$(MKDIR) "$(dir $@)"
	$(CC) "$<" $(CFLAGS) -MMD -MP -MF"$(@:.o=.d)" -MT"$@" -o "$@"

$(BUILD_DIR)/%.o: %.s Makefile
	@$(MKDIR) "$(dir $@)"
	$(CC) $(ASFLAGS) -MMD -MP -MF"$(@:.o=.d)" -MT"$@" -o "$@" "$<"

size: $(ELF)
	$(SIZE) $(ELF)

list: $(ELF)
	$(OBJDUMP) -h -S $(ELF) > "$(LIST)"

$(BIN): $(ELF)
	$(OBJCOPY) -O binary "$(ELF)" "$(BIN)"

flash: $(ELF)
	$(OPENOCD) -s "$(OPENOCD_SCRIPTS)" -f $(OPENOCD_INTERFACE) -f $(OPENOCD_TARGET) -c "program $(ELF) verify reset exit"

clean:
	-$(RM) "$(BUILD_DIR)"

-include $(DEPS)
