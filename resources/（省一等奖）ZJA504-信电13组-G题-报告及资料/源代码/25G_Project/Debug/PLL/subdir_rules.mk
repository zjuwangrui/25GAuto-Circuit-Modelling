################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
PLL/%.o: ../PLL/%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: Arm Compiler'
	"D:/APPS/CCS/ccs/tools/compiler/ti-cgt-armllvm_4.0.3.LTS/bin/tiarmclang.exe" -c @"device.opt"  -march=thumbv6m -mcpu=cortex-m0plus -mfloat-abi=soft -mlittle-endian -mthumb -O0 -I"C:/Users/elpam/workspace_ccstheia/G3519_Template_Project/FFT" -I"C:/Users/elpam/workspace_ccstheia/G3519_Template_Project/LCD_Driver" -I"C:/Users/elpam/workspace_ccstheia/G3519_Template_Project/dsp_9910" -I"C:/Users/elpam/workspace_ccstheia/G3519_Template_Project/PLL" -I"C:/Users/elpam/workspace_ccstheia/G3519_Template_Project" -I"C:/Users/elpam/workspace_ccstheia/G3519_Template_Project/Debug" -I"D:/APPS/TIM0/mspm0_sdk_2_05_01_00/source/third_party/CMSIS/Core/Include" -I"D:/APPS/TIM0/mspm0_sdk_2_05_01_00/source" -I"D:/APPS/TIM0/mspm0_sdk_2_05_01_00/source/third_party/CMSIS/DSP/Include" -gdwarf-3 -MMD -MP -MF"PLL/$(basename $(<F)).d_raw" -MT"$(@)"  $(GEN_OPTS__FLAG) -o"$@" "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


