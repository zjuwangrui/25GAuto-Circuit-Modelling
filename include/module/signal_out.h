#ifndef __MODULE_SIGNAL_OUT_H
#define __MODULE_SIGNAL_OUT_H

#include <stdint.h>
#include <stdbool.h>

/*
 * ===========================================================================
 *  signal_out —— "目标电路输出信号" 反算模块
 * ===========================================================================
 *
 *  背景 (硬件通路):
 *
 *      AD9910 ── ×N 放大 ──── H(s) ────  →  实际观测点 (示波器)
 *      (差分 Vpp)   (硬件      (二阶低通,
 *                    电路)      DC 增益 4.68)
 *
 *          H_fit(s) = 4.68 / (1.23e-8·s² + 3.48e-4·s + 1)
 *              ωn ≈ 9017 rad/s ≈ 1435 Hz (自然频率),
 *              ζ ≈ 1.57 (过阻尼), 两实极点 ~518 Hz 与 ~3990 Hz.
 *
 *  职责:
 *      给定用户想在 "示波器观测点" 看到的正弦频率和差分峰峰值,
 *      反算 AD9910 应该产生的差分峰峰值 (频率不变), 调 dds_tone_sine 输出.
 *
 *  分层理由:
 *      "×N + H(s)" 是下游硬件电路的特性, 与 AD9910 芯片本身无关.
 *      放到独立 module 里, 电路改了只需要改这一个文件.
 *
 *  用法:
 *      signal_out_init();                    // 一次性
 *      signal_out_set(1000.0, 2.0f);         // 观测点想看到 1kHz / 2Vpp
 *      // 内部: |H(1kHz)| 反算 → AD9910 vpp = target / (SIGNAL_OUT_AMP_GAIN × |H|)
 *      //       调 dds_tone_sine(1000, vpp, 0)
 *
 *
 *  超限处理:
 *      如果反算出的 AD9910 Vpp > DDS 满量程 (DDS_FULL_SCALE_V, 默认 1V),
 *      函数返回 false, 同时 UART 打印警告 + LCD 底部提示一行.
 *      (H(s) DC 增益 4.68 × 前级 SIGNAL_OUT_AMP_GAIN(5.11) ≈ 23.9,
 *       输出 ~24V 才需要 AD9910 满量程, 日常不会碰到.)
 * ===========================================================================
 */

/* 硬件电路后级放大倍数 (AD9910 输出 → H(s) 入口) */
#ifndef SIGNAL_OUT_AMP_GAIN
#define SIGNAL_OUT_AMP_GAIN     5.80f
#endif

void signal_out_init(void);

/* 主 API: 设定目标 (示波器观测点) 的频率和差分峰峰值.
 *   返回 true  = 反算成功, AD9910 已开始输出
 *   返回 false = 反算的 AD9910 幅度超过硬件极限, 未输出 (UART/LCD 已告警) */
bool signal_out_set(double freq_hz, float target_vpp);

/* 周期任务: 打印当前状态 (频率 / AD9910 vpp / |H(jω)| / 目标 vpp).
 * 注册到调度器周期性执行, 每次打一行到 USART1. */
void signal_out_task(void);

/* 只算不发: 给外部代码 (例如 UI) 查询"当前频率下要 X Vpp 需要 AD9910 出多少 Vpp",
 * 用于预校验或界面显示. */
float signal_out_calc_ad9910_vpp(double freq_hz, float target_vpp);

/* 传递函数模值 |H(j2π·f)| —— 供调试或 UI 显示"当前 f 的增益" */
float signal_out_H_mag(double freq_hz);

#endif /* __MODULE_SIGNAL_OUT_H */
