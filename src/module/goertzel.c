/*
 * goertzel.c —— Goertzel 单点 DFT 算法实现
 *
 * 见 goertzel.h 里的原理注释。这里只做实现。
 */

#include "module/goertzel.h"
#include <math.h>

/* 内部实现：返回幅度平方，可选是否开方由包装函数决定 */
static float goertzel_core_mag2(const int16_t *samples, uint32_t n,
                                uint32_t fs_hz, float target_hz)
{
    /* 归一化到 bin 索引 k = target * N / Fs，可以是小数
     * 系数只依赖 (k/N) 即只依赖 target/Fs，不必是整数 */
    const float k = target_hz * (float)n / (float)fs_hz;
    const float omega = 2.0f * 3.14159265358979323846f * k / (float)n;
    const float coeff = 2.0f * cosf(omega);

    /* 二阶差分方程递推：s0 = coeff*s1 - s2 + x[i] */
    float s1 = 0.0f, s2 = 0.0f;
    for (uint32_t i = 0; i < n; ++i) {
        float s0 = coeff * s1 - s2 + (float)samples[i];
        s2 = s1;
        s1 = s0;
    }

    /* 幅度平方 = s1² + s2² - coeff*s1*s2 */
    return s1 * s1 + s2 * s2 - coeff * s1 * s2;
}

float goertzel_magnitude2(const int16_t *samples, uint32_t n,
                          uint32_t fs_hz, float target_hz)
{
    float m2 = goertzel_core_mag2(samples, n, fs_hz, target_hz);
    return (m2 < 0.0f) ? 0.0f : m2;    /* 浮点误差可能出现极小的负值 */
}

float goertzel_magnitude(const int16_t *samples, uint32_t n,
                         uint32_t fs_hz, float target_hz)
{
    return sqrtf(goertzel_magnitude2(samples, n, fs_hz, target_hz));
}

/* 复数版：额外算一次 sinf 提取虚部。
 * 用来重建波形时需要相位，公式：
 *   Re = s1 - s2 * cos(ω)
 *   Im = s2 * sin(ω)
 */
goertzel_complex_t goertzel_complex(const int16_t *samples, uint32_t n,
                                    uint32_t fs_hz, float target_hz)
{
    const float k     = target_hz * (float)n / (float)fs_hz;
    const float omega = 2.0f * 3.14159265358979323846f * k / (float)n;
    const float cw    = cosf(omega);
    const float sw    = sinf(omega);
    const float coeff = 2.0f * cw;

    float s1 = 0.0f, s2 = 0.0f;
    for (uint32_t i = 0; i < n; ++i) {
        float s0 = coeff * s1 - s2 + (float)samples[i];
        s2 = s1;
        s1 = s0;
    }

    goertzel_complex_t r;
    r.re = s1 - s2 * cw;
    r.im = s2 * sw;
    return r;
}
