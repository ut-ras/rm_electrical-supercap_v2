################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Arm Compiler - building file: "$<"'
	"" -c @"device.opt"  -march=thumbv6m -mcpu=cortex-m0plus -mfloat-abi=soft -mlittle-endian -mthumb -O2 -I"/Users/pulunzhou/Documents/KiCad/rm_electrical-supercap_v2/TrevorsMasterpiece" -I"/Users/pulunzhou/Documents/KiCad/rm_electrical-supercap_v2/TrevorsMasterpiece/Debug" -I"/source/third_party/CMSIS/Core/Include" -I"/source" -gdwarf-3 -Wall -MMD -MP -MF"$(basename $(<F)).d_raw" -MT"$(@)"  $(GEN_OPTS__FLAG) -o"$@" "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

error.h: ../empty.syscfg
	@echo 'SysConfig - building file: "$<"'
	"/Applications/TI/ccs/utils/sysconfig_1.27.0/sysconfig_cli.sh" --script "/Users/pulunzhou/Documents/KiCad/rm_electrical-supercap_v2/TrevorsMasterpiece/empty.syscfg" -o "." --compiler ticlang
	@echo 'Finished building: "$<"'
	@echo ' '


