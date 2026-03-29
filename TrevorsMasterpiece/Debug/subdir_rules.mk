################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: Arm Compiler'
	"C:/TI/ccs2040/ccs/tools/compiler/ti-cgt-armllvm_4.0.4.LTS/bin/tiarmclang.exe" -c @"device.opt"  -march=thumbv6m -mcpu=cortex-m0plus -mfloat-abi=soft -mlittle-endian -mthumb -O2 -I"C:/Users/headf/OneDrive/Applications/KiCad/RoboMaster/rm_electrical-supercap_v2/TrevorsMasterpiece" -I"C:/Users/headf/OneDrive/Applications/KiCad/RoboMaster/rm_electrical-supercap_v2/TrevorsMasterpiece/Debug" -I"C:/TI/mspm0_sdk_2_09_00_01/source/third_party/CMSIS/Core/Include" -I"C:/TI/mspm0_sdk_2_09_00_01/source" -gdwarf-3 -Wall -MMD -MP -MF"$(basename $(<F)).d_raw" -MT"$(@)"  $(GEN_OPTS__FLAG) -o"$@" "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

build-1815107101: ../empty.syscfg
	@echo 'Building file: "$<"'
	@echo 'Invoking: SysConfig'
	"C:/TI/ccs2040/ccs/utils/sysconfig_1.26.0/sysconfig_cli.bat" -s "C:/TI/mspm0_sdk_2_09_00_01/.metadata/product.json" --script "C:/Users/headf/OneDrive/Applications/KiCad/RoboMaster/rm_electrical-supercap_v2/TrevorsMasterpiece/empty.syscfg" -o "." --compiler ticlang
	@echo 'Finished building: "$<"'
	@echo ' '

device_linker.cmd: build-1815107101 ../empty.syscfg
device.opt: build-1815107101
device.cmd.genlibs: build-1815107101
ti_msp_dl_config.c: build-1815107101
ti_msp_dl_config.h: build-1815107101
Event.dot: build-1815107101

%.o: ./%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: Arm Compiler'
	"C:/TI/ccs2040/ccs/tools/compiler/ti-cgt-armllvm_4.0.4.LTS/bin/tiarmclang.exe" -c @"device.opt"  -march=thumbv6m -mcpu=cortex-m0plus -mfloat-abi=soft -mlittle-endian -mthumb -O2 -I"C:/Users/headf/OneDrive/Applications/KiCad/RoboMaster/rm_electrical-supercap_v2/TrevorsMasterpiece" -I"C:/Users/headf/OneDrive/Applications/KiCad/RoboMaster/rm_electrical-supercap_v2/TrevorsMasterpiece/Debug" -I"C:/TI/mspm0_sdk_2_09_00_01/source/third_party/CMSIS/Core/Include" -I"C:/TI/mspm0_sdk_2_09_00_01/source" -gdwarf-3 -Wall -MMD -MP -MF"$(basename $(<F)).d_raw" -MT"$(@)"  $(GEN_OPTS__FLAG) -o"$@" "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

startup_mspm0g350x_ticlang.o: C:/TI/mspm0_sdk_2_09_00_01/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0g350x_ticlang.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: Arm Compiler'
	"C:/TI/ccs2040/ccs/tools/compiler/ti-cgt-armllvm_4.0.4.LTS/bin/tiarmclang.exe" -c @"device.opt"  -march=thumbv6m -mcpu=cortex-m0plus -mfloat-abi=soft -mlittle-endian -mthumb -O2 -I"C:/Users/headf/OneDrive/Applications/KiCad/RoboMaster/rm_electrical-supercap_v2/TrevorsMasterpiece" -I"C:/Users/headf/OneDrive/Applications/KiCad/RoboMaster/rm_electrical-supercap_v2/TrevorsMasterpiece/Debug" -I"C:/TI/mspm0_sdk_2_09_00_01/source/third_party/CMSIS/Core/Include" -I"C:/TI/mspm0_sdk_2_09_00_01/source" -gdwarf-3 -Wall -MMD -MP -MF"$(basename $(<F)).d_raw" -MT"$(@)"  $(GEN_OPTS__FLAG) -o"$@" "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


