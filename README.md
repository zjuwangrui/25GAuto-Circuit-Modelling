# 电路模型探究装置 (STM32 + AD9910 + 大彩串口屏)

STM32F103VE 平台。硬件通路：**STM32 → AD9910 → 硬件电路 (×5.11 放大器 + 未知滤波器) → 输出端**。
上位交互靠 **大彩串口屏 (USART2, 115200)**。

---

## 业务链路

### 1. 输出信号

```
串口屏 用户输入 freq/vpp (只存在屏本地, 不发 MCU)
                    │
串口屏 点 "输出信号" 按钮 ─→ 发一个组合帧: 04 11 [0x2F] [freq_ascii] ',' [vpp_ascii] 00 FF
                    │
                    ↓
MCU panel_ctrl.on_input 拆出 freq / vpp → signal_out_set(freq, vpp)
                    │
                    ↓
signal_out 用已知传递函数 H(s) + 5.11× 放大 反算, 调 dds_tone_sine
                    │
                    ↓
AD9910 → 硬件电路 → 输出信号 (freq / vpp 出现在电路末端)
```

**行为要点**：
- 用户在屏上输入 freq / vpp **不产生任何 UART 数据**，仅存在屏本地
- 点击"输出信号"按钮时，屏把 freq + vpp 一起打包成**一个帧**发给 MCU
- MCU 用逗号拆出两个值，反算传递函数后调 dds_tone_sine

### 2. 学习流程

```
串口屏 点 "学习" 按钮  ─→ MCU: learning_start()
                          ↓
learning 模块跑测试信号 → 分析响应 → 判断滤波类型
                          ↓
                    完成后: state = LEARN_DONE
                          ↓
panel_ctrl 检测到 DONE → 回推屏: 只显示 "滤波类型" 一个文本
                                  (低通/高通/带通/带阻/全通 中的一个)
```

**回传内容**：
- 只回传一个参数：**未知电路滤波类型**（字符串，例如 "LOWPASS"）
- 不回传具体参数（截止频率、Q 值等），业务不需要

### 3. 推理输出流程

```
串口屏 点 "推理" 按钮 ─→ 短触摸帧 FE [0x31] xx FF
                          ↓
                    MCU: inference_start()
                          ↓
                    inference 模块执行推理动作
                          ↓
                    (完成后不回推屏, 无屏反馈)
```

**回传内容**：无。业务不需要屏上反馈。

---

## 模块分层

```
bsp/            硬件外设 (uart, spi, gpio, adc, lcd, led)
drv/            外设芯片 (ad9910, serial_screen, esp8266, dht11)
module/         业务逻辑:
                  signal_out   传递函数反算 (freq/vpp → DDS 参数)
                  dds          AD9910 各种输出模式 (单音/RAM任意/DRG扫频/跳频/OSK)
                  panel_ctrl   串口屏胶水层 (屏事件 ↔ 业务模块)
                  learning     学习动作 (当前空壳)
                  inference    推理动作 (当前空壳)
                  ui           TFT LCD 页面
                  thd, fft, goertzel  频谱分析
core/           调度器 + 环形缓冲
```

**分层原则**：
- `drv/serial_screen` 只懂协议帧
- `module/panel_ctrl` 只懂 UI 业务映射
- `module/learning` / `module/inference` 只懂算法
- 换屏 → 只改 `drv/serial_screen`；改算法 → 只改对应 module

---

## 硬件接线

| 功能 | STM32 引脚 | 备注 |
|---|:---:|---|
| SPI1 → AD9910 | PA5 / PA6 / PA7 | SCK / MISO(未接) / MOSI |
| AD9910 CS | PA4 | |
| AD9910 RST | PC3 | |
| AD9910 IOUP | PA12 | |
| AD9910 PROFILE[0..2] | PB12 / PB13 / PB14 | |
| AD9910 PWR/DPH/DRC | PC0 / PC1 / PC2 | 上电拉低保命 |
| **USART2 → 串口屏** | **PA2 (TX) / PA3 (RX)** | **115200 8N1** |
| USART1 → 调试口 | PA9 / PA10 | 115200，UART_Printf |
| LCD (FSMC) | 一堆 | 保持原样 |

详细接线：`resources/AD9910模块-STM32接线说明(按实物更正版).txt`

---

## 传递函数（signal_out 用）

$$H_{fit}(s) = \frac{4.68}{1.23 \times 10^{-8} s^2 + 3.48 \times 10^{-4} s + 1}$$

- ωn ≈ 9017 rad/s ≈ 1435 Hz，ζ ≈ 1.57（过阻尼，两实极点 ~518 Hz / ~3990 Hz）
- DC 增益 = 4.68，前级放大 5.00× → **总 DC 增益 ≈ 23.4**
- 系数在 [src/module/signal_out.c](src/module/signal_out.c) 里 `H_S_COEF_*` / `H_S_DC_GAIN` 三个宏
- 放大倍数在 [include/module/signal_out.h](include/module/signal_out.h) 里 `SIGNAL_OUT_AMP_GAIN`


---

---

## 构建

PlatformIO，配置在 [platformio.ini](platformio.ini)：

```shell
pio run              # 编译
pio run -t upload    # 烧录 (ST-Link)
pio device monitor   # 打开串口 (USART1)
```
