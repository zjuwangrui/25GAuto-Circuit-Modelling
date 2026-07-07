#include "drv/dht11.h"

/* ===== DWT 微秒延时 ===== */
static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_us(uint32_t us)
{
    uint32_t start  = DWT->CYCCNT;
    uint32_t cycles = us * (SystemCoreClock / 1000000U);
    while ((DWT->CYCCNT - start) < cycles);
}

/* ===== GPIO 方向切换 ===== */
static void DHT11_Mode_Out_PP(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin   = DHT11_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_PORT, &g);
}

static void DHT11_Mode_IPU(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin  = DHT11_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DHT11_PORT, &g);
}

#define DHT11_Dout_1    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET)
#define DHT11_Dout_0    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET)
#define DHT11_Dout_IN() HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN)

/* 超时计数上限：约等于 500µs（72MHz 下每次循环 ~3 cycles） */
#define DHT11_WAIT_LIMIT 12000U

/* ===== 读一字节 MSB 先，含超时保护 ===== */
static uint8_t DHT11_ReadByte(void)
{
    uint8_t i, val = 0;
    uint32_t t;

    for (i = 0; i < 8; i++) {
        /* 等每 bit 起始 50µs 低电平结束 */
        t = 0;
        while (DHT11_Dout_IN() == GPIO_PIN_RESET && ++t < DHT11_WAIT_LIMIT);

        /* 延时 40µs 后采样 */
        delay_us(40);

        if (DHT11_Dout_IN() == GPIO_PIN_SET) {
            val |= (uint8_t)(0x80u >> i);
            /* 等 '1' 的高电平结束 */
            t = 0;
            while (DHT11_Dout_IN() == GPIO_PIN_SET && ++t < DHT11_WAIT_LIMIT);
        }
    }
    return val;
}

/* ===== 公开接口 ===== */

void DHT11_Init(void)
{
    __HAL_RCC_GPIOE_CLK_ENABLE();
    dwt_init();
    DHT11_Mode_Out_PP();
    DHT11_Dout_1;
    HAL_Delay(1);
}

HAL_StatusTypeDef DHT11_Read(DHT11_Data_t *out)
{
    uint32_t t;
    uint8_t humi_int, humi_deci, temp_int, temp_deci, checksum;

    /* 1. 起始信号：拉低 18ms，拉高 30µs */
    DHT11_Mode_Out_PP();
    DHT11_Dout_0;
    HAL_Delay(18);
    DHT11_Dout_1;
    delay_us(30);

    /* 2. 切输入，判断 DHT11 应答 */
    DHT11_Mode_IPU();

    if (DHT11_Dout_IN() == GPIO_PIN_RESET) {
        /* 等过 ~80µs 低电平应答 */
        t = 0;
        while (DHT11_Dout_IN() == GPIO_PIN_RESET && ++t < DHT11_WAIT_LIMIT);

        /* 等过 ~80µs 高电平准备 */
        t = 0;
        while (DHT11_Dout_IN() == GPIO_PIN_SET  && ++t < DHT11_WAIT_LIMIT);

        /* 3. 读 40bit */
        humi_int  = DHT11_ReadByte();
        humi_deci = DHT11_ReadByte();
        temp_int  = DHT11_ReadByte();
        temp_deci = DHT11_ReadByte();
        checksum  = DHT11_ReadByte();

        /* 4. 恢复输出高 */
        DHT11_Mode_Out_PP();
        DHT11_Dout_1;

        /* 5. 校验 */
        if (checksum == (uint8_t)(humi_int + humi_deci + temp_int + temp_deci)) {
            out->temp_int  = temp_int;
            out->temp_deci = temp_deci;
            out->humi_int  = humi_int;
            out->humi_deci = humi_deci;
            return HAL_OK;
        }
        return HAL_ERROR;
    }

    return HAL_ERROR;
}
