################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Arm Compiler - building file: "$<"'
	"E:/CCSVScode/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin/tiarmclang.exe" -c @"syscfg/device.opt"  -march=thumbv6m -mcpu=cortex-m0plus -mfloat-abi=soft -mlittle-endian -mthumb -O2 -I"C:/Users/xiazh/workspace_ccstheia/empty_mspm0g3507" -I"C:/Users/xiazh/workspace_ccstheia/empty_mspm0g3507/Debug" -I"C:/TI/mspm0_sdk_2_11_00_07/source/third_party/CMSIS/Core/Include" -I"C:/TI/mspm0_sdk_2_11_00_07/source" -gdwarf-3 -MMD -MP -MF"$(basename $(<F)).d_raw" -MT"$(@)" -I"C:/Users/xiazh/workspace_ccstheia/empty_mspm0g3507/Debug/syscfg"  $(GEN_OPTS__FLAG) -o"$@" "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

build-1873967390: ../empty_mspm0g3507.syscfg
	@echo 'SysConfig - building file: "$<"'
	"C:/TI/sysconfig_1.26.2/sysconfig_cli.bat" -s "C:/TI/mspm0_sdk_2_11_00_07/.metadata/product.json" --script "C:/Users/xiazh/workspace_ccstheia/empty_mspm0g3507/empty_mspm0g3507.syscfg" -o "syscfg" --compiler ticlang
	@echo 'Finished building: "$<"'
	@echo ' '

syscfg/device_linker.cmd: build-1873967390 ../empty_mspm0g3507.syscfg
syscfg/device.opt: build-1873967390
syscfg/device.cmd.genlibs: build-1873967390
syscfg/ti_msp_dl_config.c: build-1873967390
syscfg/ti_msp_dl_config.h: build-1873967390
syscfg/Event.dot: build-1873967390
syscfg: build-1873967390

syscfg/%.o: ./syscfg/%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Arm Compiler - building file: "$<"'
	"E:/CCSVScode/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin/tiarmclang.exe" -c @"syscfg/device.opt"  -march=thumbv6m -mcpu=cortex-m0plus -mfloat-abi=soft -mlittle-endian -mthumb -O2 -I"C:/Users/xiazh/workspace_ccstheia/empty_mspm0g3507" -I"C:/Users/xiazh/workspace_ccstheia/empty_mspm0g3507/Debug" -I"C:/TI/mspm0_sdk_2_11_00_07/source/third_party/CMSIS/Core/Include" -I"C:/TI/mspm0_sdk_2_11_00_07/source" -gdwarf-3 -MMD -MP -MF"syscfg/$(basename $(<F)).d_raw" -MT"$(@)" -I"C:/Users/xiazh/workspace_ccstheia/empty_mspm0g3507/Debug/syscfg"  $(GEN_OPTS__FLAG) -o"$@" "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

startup_mspm0g350x_ticlang.o: C:/TI/mspm0_sdk_2_11_00_07/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0g350x_ticlang.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Arm Compiler - building file: "$<"'
	"E:/CCSVScode/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin/tiarmclang.exe" -c @"syscfg/device.opt"  -march=thumbv6m -mcpu=cortex-m0plus -mfloat-abi=soft -mlittle-endian -mthumb -O2 -I"C:/Users/xiazh/workspace_ccstheia/empty_mspm0g3507" -I"C:/Users/xiazh/workspace_ccstheia/empty_mspm0g3507/Debug" -I"C:/TI/mspm0_sdk_2_11_00_07/source/third_party/CMSIS/Core/Include" -I"C:/TI/mspm0_sdk_2_11_00_07/source" -gdwarf-3 -MMD -MP -MF"$(basename $(<F)).d_raw" -MT"$(@)" -I"C:/Users/xiazh/workspace_ccstheia/empty_mspm0g3507/Debug/syscfg"  $(GEN_OPTS__FLAG) -o"$@" "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


