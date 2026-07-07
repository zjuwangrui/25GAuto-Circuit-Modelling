#ifndef __MODULE_GOERTZEL_H
#define __MODULE_GOERTZEL_H

#include <stdint.h>

/*
 * ===========================================================================
 *  Goertzel 算法（单点 DFT）
 * ===========================================================================
 *
 * 一次只算某一个目标频率处的复数幅值，代价 O(N)。相比于 FFT：
 *   - FFT 一次算 N/2 个 bin，任意 bin 免费；
 *   - Goertzel 每次固定 3 次乘加，只算你要的那一个频率。
 * 因此当"想看的频点很少（比如 5 个谐波）"时，Goertzel 明显更划算，
 * 而且频率不受 bin 网格 Δf=Fs/N 约束，任意频率都能算。
 *
 * ---------------------------------------------------------------------------
 * 递推公式（浮点版，为可读性）：
 *
 *     k     = target_hz * N / Fs             (target 归一化到 bin 索引)
 *     ω     = 2π * k / N
 *     coeff = 2 * cos(ω)
 *
 *     s0 = coeff * s1 - s2 + x[n]            对 n=0..N-1 迭代
 *     s2 = s1
 *     s1 = s0
 *
 *     |X(k)|² = s1² + s2² - coeff * s1 * s2
 *
 *   （幅值 = sqrt(|X(k)|²)，若只比较大小可以不开根）
 *
 * ---------------------------------------------------------------------------
 * 使用注意：
 *   - 输入是 int16_t，我们不做加窗（Goertzel 对纯音频率天然定位准）；
 *     如果想抑制泄漏，可在调用前自己乘 Hanning。
 *   - 结果没做归一化。要跨帧比较相对幅度，只除以 N 或者除以基频幅度即可。
 *   - target_hz 必须 < Fs/2，否则违反 Nyquist，返回值无意义。
 * ===========================================================================
 */

/* 单点 Goertzel：返回 target_hz 处的幅度（未开根的平方值可用 _mag2 版本）。
 *   samples : 长度 n 的 int16_t 数据（无需去直流；DC 只影响 0Hz bin）
 *   n       : 样本数
 *   fs_hz   : 采样率
 *   target_hz : 目标频率 (Hz)，需 < fs_hz/2
 * 返回：|X(target)|，单位与输入同量纲 */
float goertzel_magnitude (const int16_t *samples, uint32_t n,
                          uint32_t fs_hz, float target_hz);

/* 幅度平方版本：省一次 sqrtf，适合只比较大小 / 找峰的场景 */
float goertzel_magnitude2(const int16_t *samples, uint32_t n,
                          uint32_t fs_hz, float target_hz);

/* 复数版本：同时返回实部/虚部。用于需要相位的场景（例如波形重建）。
 *   Re(X) = s1 - s2·cos(ω)
 *   Im(X) = s2·sin(ω)
 *   |X|   = sqrt(Re² + Im²)
 *   ∠X   = atan2(Im, Re)  (radians, -π..π)
 */
typedef struct {
    float re;
    float im;
} goertzel_complex_t;

goertzel_complex_t goertzel_complex(const int16_t *samples, uint32_t n,
                                    uint32_t fs_hz, float target_hz);

#endif /* __MODULE_GOERTZEL_H */
