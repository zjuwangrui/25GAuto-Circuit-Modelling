# 电路模型探究装置 (STM32 + AD9910 + 大彩串口屏)

STM32F103VE 平台。硬件通路：**STM32 → AD9910 → 硬件电路 (×5.11 放大器 + 未知滤波器) → 输出端**。
上位交互靠 **大彩串口屏 (USART2, 115200)**。

---

## 业务链路

### 1. 输出信号（当前唯一实现的链路）

屏侧 3 个控件（学习/推理按钮暂缓）：

| 控件 | 类型 | ctrl_id | 屏发/收帧 |
|---|---|:---:|---|
| 频率输入框 | 文本 (type=0x11) | **4** | `EE B1 11 00 00 00 04 11 [ASCII] 00 FF FC FF FF` |
| 电压输入框 | 文本 (type=0x11) | **7** | `EE B1 11 00 00 00 07 11 [ASCII] 00 FF FC FF FF` |
| 输出信号按钮 | 按钮 (type=0x10) | **11** | `EE B1 11 00 00 00 0B 10 00 01` (**10 字节, 无 FF FC FF FF 帧尾**) |

```
用户屏上输入 freq   → 屏发文本帧 (ctrl_id=4)  → MCU 缓存 last_freq
用户屏上输入 vpp    → 屏发文本帧 (ctrl_id=7)  → MCU 缓存 last_vpp
用户点 "输出信号"   → 屏发按钮帧 (ctrl_id=11) → MCU 调 signal_out_set(last_freq, last_vpp)
                                                → signal_out 反算 H(s)+5.80× → dds_tone_sine
                                                → AD9910 → 电路 → OUT
```

### 2. 学习流程（TODO）

屏侧学习按钮暂未定义 ctrl_id。业务模块 `module/learning` 保留空壳（`learning_start` 只置状态）。

### 3. 推理流程（TODO）

同上，`module/inference` 保留空壳。

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
