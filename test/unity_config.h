// test/unity_config.h
#ifndef UNITY_CONFIG_H
#define UNITY_CONFIG_H

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// 使用串口输出测试结果
#define UNITY_OUTPUT_CHAR(c) HAL_UART_Transmit(&huart1, (uint8_t*)&c, 1, 100)

// 可选：启用浮点数比较
#define UNITY_INCLUDE_DOUBLE
#define UNITY_DOUBLE_PRECISION 0.0001

// 可选：颜色输出
#define UNITY_OUTPUT_COLOR

#ifdef __cplusplus
}
#endif

#endif /* UNITY_CONFIG_H */