#include "drv/esp8266.h"
#include "bsp/uart.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "stm32f1xx_hal.h"
/* ===== 内部接收环形缓冲区 ===== */
#define RX_BUF_SIZE 1024U

static uint8_t  rx_ring[RX_BUF_SIZE];
static volatile uint32_t rx_head = 0; // 写指针（中断写入）
static volatile uint32_t rx_tail = 0; // 读指针（主循环读取）
static volatile uint8_t rx_byte_isr;  /* 中断写入，必须 volatile */

ESP_Stats g_esp_stats = {0};

/* ===== 中断接口 ===== */
void ESP_UART_IRQHandler(void)
{
    rx_ring[rx_head % RX_BUF_SIZE] = rx_byte_isr;
    rx_head++;
    HAL_UART_Receive_IT(&ESP_UART, (uint8_t *)&rx_byte_isr, 1);
}

/* ===== 内部工具函数 ===== */

static void rx_clear(void)
{
    rx_tail = rx_head;
}

static uint32_t rx_available(void)
{
    return rx_head - rx_tail;
}

static uint8_t rx_pop(void)
{
    return rx_ring[(rx_tail++) % RX_BUF_SIZE];
}

static void esp_send_raw(const char *s)
{
    HAL_UART_Transmit(&ESP_UART, (const uint8_t *)s, strlen(s), 2000);
}

/**
 * 等待 buf 中出现 expect 字符串，超时返回 ESP_ERR_TIMEOUT
 * 期间如出现 "ERROR" 或 "FAIL" 立即返回 ESP_ERR_AT
 */
static ESP_Result esp_wait_for(const char *expect, uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    char acc[256];
    uint32_t idx = 0;

    while (HAL_GetTick() - start < timeout_ms) {
        while (rx_available() > 0 && idx < sizeof(acc) - 1) {
            acc[idx++] = (char)rx_pop();
            acc[idx]   = '\0';
        }
        if (strstr(acc, expect))   return ESP_OK;
        if (strstr(acc, "ERROR")) {
            UART_Printf("[ESP] ERR  cmd=\"%s\" raw=[%.*s]\r\n", expect, (int)idx, acc);
            return ESP_ERR_AT;
        }
        if (strstr(acc, "FAIL")) {
            UART_Printf("[ESP] FAIL cmd=\"%s\" raw=[%.*s]\r\n", expect, (int)idx, acc);
            return ESP_ERR_AT;
        }
        /* 服务端主动断连：不必等满超时 */
        if (strstr(acc, "CLOSED"))  return ESP_ERR_TIMEOUT;
    }
    if (idx == 0)
        UART_Printf("[ESP] TIMEOUT waiting \"%s\" — 无响应（检查接线/波特率）\r\n", expect);
    else
        UART_Printf("[ESP] TIMEOUT waiting \"%s\" raw=[%.*s]\r\n", expect, (int)idx, acc);
    return ESP_ERR_TIMEOUT;
}

/* 关闭 TCP 连接——连接已被服务端关闭时 AT+CIPCLOSE 会返回 ERROR，属正常，静默处理 */
static void esp_cipclose(void)
{
    rx_clear();
    esp_send_raw("AT+CIPCLOSE\r\n");
    /* 等 OK 或 ERROR，都可以继续，不打印错误 */
    uint32_t start = HAL_GetTick();
    char acc[64]; uint32_t idx = 0;
    while (HAL_GetTick() - start < 1000) {
        while (rx_available() > 0 && idx < sizeof(acc) - 1) {
            acc[idx++] = (char)rx_pop();
            acc[idx]   = '\0';
        }
        if (strstr(acc, "OK") || strstr(acc, "ERROR")) break;
    }
    HAL_Delay(200); // 等模块处理完关闭指令
}

/**
 * 发送 AT 命令并等待期望响应
 * 自动补\r\n
 */
static ESP_Result esp_cmd(const char *cmd, const char *expect, uint32_t timeout_ms)
{
    rx_clear();
    esp_send_raw(cmd);
    esp_send_raw("\r\n");
    return esp_wait_for(expect, timeout_ms);
}

/**
 * 继续读取数据直到超过 idle_ms 没有新数据（或缓冲满）
 * 用于读取HTTP响应
 */
static void esp_read_until_idle(char *buf, uint32_t buf_size, uint32_t max_wait_ms, uint32_t idle_ms)
{
    uint32_t total_start = HAL_GetTick();
    uint32_t last_rx     = HAL_GetTick();
    uint32_t idx = 0;

    while (HAL_GetTick() - total_start < max_wait_ms && idx < buf_size - 1) {
        if (rx_available() > 0) {
            buf[idx++] = (char)rx_pop();
            buf[idx]   = '\0';
            last_rx = HAL_GetTick();
        } else if (idx > 0 && HAL_GetTick() - last_rx > idle_ms) {
            break; // 有数据且空闲超过idle_ms，认为接收完毕
        }
    }
}

/* ===== 公开接口实现 ===== */

ESP_Result ESP_Init(void)
{
    /* ---- 底层 UART 初始化（幂等，重复调用无副作用） ---- */
    MX_USART3_UART_Init();

    /* ---- 硬件初始化：CH_PD / RST 引脚 ---- */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = ESP_CH_PD_PIN | ESP_RST_PIN;  /* PB8, PB9 */
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);

    /* 硬件复位：RST 拉低 200ms 再拉高，CH_PD 保持高电平 */
    ESP_CH_ENABLE();
    ESP_RST_LOW();
    HAL_Delay(200);
    ESP_RST_HIGH();

    /* 启动中断接收，等待模块上电稳定 */
    HAL_UART_Receive_IT(&ESP_UART, (uint8_t *)&rx_byte_isr, 1);
    /* 等 "ready" 字符串，最长 5s；旧固件不发此字符串则兜底等 3s */
    if (esp_wait_for("ready", 5000) != ESP_OK)
        HAL_Delay(3000);
    rx_clear();

    // 测试 AT 通信
    if (esp_cmd("AT", "OK", ESP_TIMEOUT_SHORT) != ESP_OK)
        return ESP_ERR_AT;

    // 关闭回显，减少噪声
    esp_cmd("ATE0", "OK", ESP_TIMEOUT_SHORT);

    // 设置AP模式
    if (esp_cmd("AT+CWMODE=2", "OK", ESP_TIMEOUT_SHORT) != ESP_OK)
        return ESP_ERR_AT;

    // 建立热点
    char cwsap_cmd[96];
    snprintf(cwsap_cmd, sizeof(cwsap_cmd),
             "AT+CWSAP=\"%s\",\"%s\",6,3", AP_SSID, AP_PASS);
    if (esp_cmd(cwsap_cmd, "OK", ESP_TIMEOUT_LONG) != ESP_OK)
        return ESP_ERR_AT;

    // 单连接模式（STM32做客户端，每次POST后关闭）
    if (esp_cmd("AT+CIPMUX=0", "OK", ESP_TIMEOUT_SHORT) != ESP_OK)
        return ESP_ERR_AT;

    g_esp_stats.ap_ready = true;
    return ESP_OK;
}

ESP_Result ESP_PostSensor(float voltage, bool led_on, uint32_t light,
                          uint8_t temp,
                          float *out_thr, uint32_t *out_light_thr,
                          uint8_t *out_temp_thr)
{
    static char body[72];
    static char request[256];
    static char cmd_buf[48];
    static char response[768];

    // 构造JSON body
    snprintf(body, sizeof(body),
             "{\"v\":%.3f,\"led\":%d,\"light\":%lu,\"temp\":%d}",
             voltage, led_on ? 1 : 0, (unsigned long)light, (int)temp);
    int body_len = (int)strlen(body);

    // 构造HTTP POST请求
    int req_len = snprintf(request, sizeof(request),
        "POST /sensor HTTP/1.0\r\n"
        "Host: %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s",
        PHONE_IP, body_len, body);
    /* snprintf 截断保护：截断时 req_len >= sizeof(request) */
    if (req_len <= 0 || (size_t)req_len >= sizeof(request))
        return ESP_ERR_AT;

    /* 1. 建立TCP连接 */
    snprintf(cmd_buf, sizeof(cmd_buf),
             "AT+CIPSTART=\"TCP\",\"%s\",%d", PHONE_IP, PHONE_PORT);
    rx_clear();
    esp_send_raw(cmd_buf);
    esp_send_raw("\r\n");
    // 等待 CONNECT 或 OK
    if (esp_wait_for("CONNECT", ESP_TIMEOUT_SHORT) != ESP_OK) {
        esp_cipclose();
        g_esp_stats.fail_count++;
        g_esp_stats.last_send_ok = false;
        return ESP_ERR_TIMEOUT;
    }

    /* 2. 告知发送长度，等待 > 提示符 */
    rx_clear();
    snprintf(cmd_buf, sizeof(cmd_buf), "AT+CIPSEND=%d", req_len);
    esp_send_raw(cmd_buf);
    esp_send_raw("\r\n");
    if (esp_wait_for(">", ESP_TIMEOUT_SHORT) != ESP_OK) {
        esp_cipclose();
        g_esp_stats.fail_count++;
        g_esp_stats.last_send_ok = false;
        return ESP_ERR_TIMEOUT;
    }

    /* 3. 发送HTTP请求体 */
    rx_clear();
    HAL_UART_Transmit(&ESP_UART, (const uint8_t *)request, req_len, 2000);

    /* 4. 等待 +IPD（本地WiFi延迟极低，SEND OK 和 +IPD 几乎同时到达）
     *    若分两步等 "SEND OK" 再等 "+IPD"，第一步会把 +IPD 消费掉导致第二步超时
     *    直接等 +IPD 即可；若发送失败 ESP8266 会回 ERROR，esp_wait_for 会捕获 */
    if (esp_wait_for("+IPD", 6000) != ESP_OK) {
        esp_cipclose();
        g_esp_stats.fail_count++;
        g_esp_stats.last_send_ok = false;
        return ESP_ERR_TIMEOUT;
    }

    /* 5. 读取完整响应（等待200ms空闲） */
    memset(response, 0, sizeof(response));
    esp_read_until_idle(response, sizeof(response), 3000, 200);

    /* 6. 关闭连接 */
    esp_cipclose();

    /* 7. 从HTTP响应中解析JSON body
     *    期望格式: {"thr":1.500,"light_thr":2000}
     *    两个字段均为可选，缺失时保持调用方传入的原值 */
    char *json_start = strchr(response, '{');
    if (json_start) {
        if (out_thr) {
            char *thr_key = strstr(json_start, "\"thr\":");
            if (thr_key) {
                float val = strtof(thr_key + 6, NULL);
                if (val > 0.0f && val <= 3.3f)
                    *out_thr = val;
            }
        }
        if (out_light_thr) {
            char *lt_key = strstr(json_start, "\"light_thr\":");
            if (lt_key) {
                long val = strtol(lt_key + 12, NULL, 10);
                if (val >= 0 && val <= 4095)
                    *out_light_thr = (uint32_t)val;
            }
        }
        if (out_temp_thr) {
            char *tt_key = strstr(json_start, "\"temp_thr\":");
            if (tt_key) {
                long val = strtol(tt_key + 11, NULL, 10);
                if (val >= 0 && val <= 50)
                    *out_temp_thr = (uint8_t)val;
            }
        }
    }

    g_esp_stats.send_count++;
    g_esp_stats.last_send_ok = true;
    return ESP_OK;
}
