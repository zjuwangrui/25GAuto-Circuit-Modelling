################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: Arm Compiler'
	"D:/APPS/CCS/ccs/tools/compiler/ti-cgt-armllvm_4.0.3.LTS/bin/tiarmclang.exe" -c @"device.opt"  -march=thumbv6m -mcpu=cortex-m0plus -mfloat-abi=soft -mlittle-endian -mthumb -O0 -I"C:/Users/elpam/workspace_ccstheia/25G_Project/FFT" -I"C:/Users/elpam/workspace_ccstheia/25G_Project/LCD_Driver" -I"C:/Users/elpam/workspace_ccstheia/25G_Project/dsp_9910" -I"C:/Users/elpam/workspace_ccstheia/25G_Project/PLL" -I"C:/Users/elpam/workspace_ccstheia/25G_Project" -I"C:/Users/elpam/workspace_ccstheia/25G_Project/Debug" -I"D:/APPS/TIM0/mspm0_sdk_2_05_01_00/source/third_party/CMSIS/Core/Include" -I"D:/APPS/TIM0/mspm0_sdk_2_05_01_00/source" -I"D:/APPS/TIM0/mspm0_sdk_2_05_01_00/source/third_party/CMSIS/DSP/Include" -gdwarf-3 -MMD -MP -MF"$(basename $(<F)).d_raw" -MT"$(@)"  $(GEN_OPTS__FLAG) -o"$@" "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

build-1075773583: ../adc12_simultaneous_trigger_event.syscfg
	@echo 'Building file: "$<"'
	@echo 'Invoking: SysConfig'
	"D:/APPS/CCS/ccs/utils/sysconfig_1.24.0/sysconfig_cli.bat" --script "C:/Users/elpam/workspace_ccstheia/25G_Project/adc12_simultaneous_trigger_event.syscfg" -o "." -s "D:/APPS/TIM0/mspm0_sdk_2_05_01_00/.metadata/product.json" --compiler ticlang
	@echo 'Finished building: "$<"'
	@echo ' '

device_linker.cmd: build-1075773583 ../adc12_simultaneous_trigger_event.syscfg
device.opt: build-1075773583
device.cmd.genlibs: build-1075773583
ti_msp_dl_config.c: build-1075773583
ti_msp_dl_config.h: build-1075773583
Event.dot: build-1075773583

%.o: ./%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: Arm Compiler'
	"D:/APPS/CCS/ccs/tools/compiler/ti-cgt-armllvm_4.0.3.LTS/bin/tiarmclang.exe" -c @"device.opt"  -march=thumbv6m -mcpu=cortex-m0plus -mfloat-abi=soft -mlittle-endian -mthumb -O0 -I"C:/Users/elpam/workspace_ccstheia/25G_Project/FFT" -I"C:/Users/elpam/workspace_ccstheia/25G_Project/LCD_Driver" -I"C:/Users/elpam/workspace_ccstheia/25G_Project/dsp_9910" -I"C:/Users/elpam/workspace_ccstheia/25G_Project/PLL" -I"C:/Users/elpam/workspace_ccstheia/25G_Project" -I"C:/Users/elpam/workspace_ccstheia/25G_Project/Debug" -I"D:/APPS/TIM0/mspm0_sdk_2_05_01_00/source/third_party/CMSIS/Core/Include" -I"D:/APPS/TIM0/mspm0_sdk_2_05_01_00/source" -I"D:/APPS/TIM0/mspm0_sdk_2_05_01_00/source/third_party/CMSIS/DSP/Include" -gdwarf-3 -MMD -MP -MF"$(basename $(<F)).d_raw" -MT"$(@)"  $(GEN_OPTS__FLAG) -o"$@" "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

startup_mspm0g351x_ticlang.o: D:/APPS/TIM0/mspm0_sdk_2_05_01_00/source/ti/devices/msp/m0p/startup_system_files/ticlang/startup_mspm0g351x_ticlang.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: Arm Compiler'
	"D:/APPS/CCS/ccs/tools/compiler/ti-cgt-armllvm_4.0.3.LTS/bin/tiarmclang.exe" -c @"device.opt"  -march=thumbv6m -mcpu=cortex-m0plus -mfloat-abi=soft -mlittle-endian -mthumb -O0 -I"C:/Users/elpam/workspace_ccstheia/25G_Project/FFT" -I"C:/Users/elpam/workspace_ccstheia/25G_Project/LCD_Driver" -I"C:/Users/elpam/workspace_ccstheia/25G_Project/dsp_9910" -I"C:/Users/elpam/workspace_ccstheia/25G_Project/PLL" -I"C:/Users/elpam/workspace_ccstheia/25G_Project" -I"C:/Users/elpam/workspace_ccstheia/25G_Project/Debug" -I"D:/APPS/TIM0/mspm0_sdk_2_05_01_00/source/third_party/CMSIS/Core/Include" -I"D:/APPS/TIM0/mspm0_sdk_2_05_01_00/source" -I"D:/APPS/TIM0/mspm0_sdk_2_05_01_00/source/third_party/CMSIS/DSP/Include" -gdwarf-3 -MMD -MP -MF"$(basename $(<F)).d_raw" -MT"$(@)"  $(GEN_OPTS__FLAG) -o"$@" "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


