################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

CG_TOOL_ROOT := E:/CCSVScode/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS

GEN_OPTS__FLAG := @"syscfg/device.opt" 
GEN_CMDS__FLAG := -Wl,-l"syscfg/device_linker.cmd" 

ORDERED_OBJS += \
"./empty_mspm0g3507.o" \
"./syscfg/ti_msp_dl_config.o" \
"./startup_mspm0g350x_ticlang.o" \
"./encoder.o" \
"./init.o" \
"./key.o" \
"./oled.o" \
"./pages.o" \
"./servo.o" \
"./task_manager.o" \
"./tracking.o" \
"./uart.o" \
$(GEN_CMDS__FLAG) \
-Wl,-ldevice.cmd.genlibs \
-Wl,-llibc.a \

-include ../makefile.init

RM := DEL /F
RMDIR := RMDIR /S/Q

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir_vars.mk
-include subdir_rules.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C55_DEPS)),)
-include $(C55_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
ifneq ($(strip $(S67_DEPS)),)
-include $(S67_DEPS)
endif
ifneq ($(strip $(S62_DEPS)),)
-include $(S62_DEPS)
endif
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(OPT_DEPS)),)
-include $(OPT_DEPS)
endif
ifneq ($(strip $(C??_DEPS)),)
-include $(C??_DEPS)
endif
ifneq ($(strip $(ASM_UPPER_DEPS)),)
-include $(ASM_UPPER_DEPS)
endif
ifneq ($(strip $(S??_DEPS)),)
-include $(S??_DEPS)
endif
ifneq ($(strip $(C64_DEPS)),)
-include $(C64_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(S64_DEPS)),)
-include $(S64_DEPS)
endif
ifneq ($(strip $(CLA_DEPS)),)
-include $(CLA_DEPS)
endif
ifneq ($(strip $(S55_DEPS)),)
-include $(S55_DEPS)
endif
ifneq ($(strip $(SV7A_DEPS)),)
-include $(SV7A_DEPS)
endif
ifneq ($(strip $(C62_DEPS)),)
-include $(C62_DEPS)
endif
ifneq ($(strip $(C67_DEPS)),)
-include $(C67_DEPS)
endif
ifneq ($(strip $(K_DEPS)),)
-include $(K_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(C43_DEPS)),)
-include $(C43_DEPS)
endif
ifneq ($(strip $(S43_DEPS)),)
-include $(S43_DEPS)
endif
ifneq ($(strip $(ASM_DEPS)),)
-include $(ASM_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
ifneq ($(strip $(SA_DEPS)),)
-include $(SA_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
EXE_OUTPUTS += \
empty_mspm0g3507.out 

EXE_OUTPUTS__QUOTED += \
"empty_mspm0g3507.out" 

BIN_OUTPUTS += \
empty_mspm0g3507.hex 

BIN_OUTPUTS__QUOTED += \
"empty_mspm0g3507.hex" 


# All Target
all: $(OBJS) $(GEN_CMDS)
	@$(MAKE) --no-print-directory -Onone "empty_mspm0g3507.out" secondary-outputs

# Tool invocations
empty_mspm0g3507.out: $(OBJS) $(GEN_CMDS)
	@echo 'Arm Linker - building target: "$@"'
	"E:/CCSVScode/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin/tiarmclang.exe" @"syscfg/device.opt"  -march=thumbv6m -mcpu=cortex-m0plus -mfloat-abi=soft -mlittle-endian -mthumb -O2 -gdwarf-3 -Wl,-m"empty_mspm0g3507.map" -Wl,-i"C:/TI/mspm0_sdk_2_11_00_07/source" -Wl,-i"C:/Users/xiazh/workspace_ccstheia/empty_mspm0g3507/Debug/syscfg" -Wl,-i"E:/CCSVScode/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/lib" -Wl,--diag_wrap=off -Wl,--display_error_number -Wl,--warn_sections -Wl,--xml_link_info="empty_mspm0g3507_linkInfo.xml" -Wl,--rom_model -o "empty_mspm0g3507.out" $(ORDERED_OBJS)
	@echo 'Finished building target: "$@"'
	@echo ' '

empty_mspm0g3507.hex: $(EXE_OUTPUTS)
	@echo 'Arm Hex Utility - building secondary target: "$@"'
	"E:/CCSVScode/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin/tiarmhex.exe" --memwidth=8 --romwidth=8 --diag_wrap=off --intel -o "empty_mspm0g3507.hex" $(EXE_OUTPUTS__QUOTED)
	@echo 'Finished building secondary target: "$@"'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(GEN_MISC_FILES__QUOTED)$(BIN_OUTPUTS__QUOTED)$(GEN_FILES__QUOTED)$(EXE_OUTPUTS__QUOTED)
	-$(RM) "empty_mspm0g3507.o" "syscfg\ti_msp_dl_config.o" "startup_mspm0g350x_ticlang.o" "encoder.o" "init.o" "key.o" "oled.o" "pages.o" "servo.o" "task_manager.o" "tracking.o" "uart.o" 
	-$(RM) "empty_mspm0g3507.d" "syscfg\ti_msp_dl_config.d" "startup_mspm0g350x_ticlang.d" "encoder.d" "init.d" "key.d" "oled.d" "pages.d" "servo.d" "task_manager.d" "tracking.d" "uart.d" 
	-$(RMDIR) $(GEN_MISC_DIRS__QUOTED)
	-@echo ' '

secondary-outputs: $(BIN_OUTPUTS)

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets

