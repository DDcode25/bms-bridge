#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "pack_model.h"
#include "pylon_cmd.h"
#include "pylon_frame.h"
#include "pylon_rs485.h"

static const char *TAG = "pylon485";

#define UART_PORT     ((uart_port_t)CONFIG_PYLON_UART_NUM)
#define REQ_MAX       128     /* запросы инвертора короткие: заголовок + 2 символа INFO */
#define RESP_MAX      512
#define SILENCE_US    ((int64_t)CONFIG_PYLON_SILENCE_S * 1000000)
#define NO_POLL_US    (30LL * 1000000)   /* T5: потеря опроса инвертором */

#ifdef CONFIG_PYLON_CURRENT_10MA
#define CURRENT_UNIT_MA 10
#else
#define CURRENT_UNIT_MA 100
#endif

#ifdef CONFIG_PYLON_DCL_NEGATIVE
#define DCL_NEGATIVE true
#else
#define DCL_NEGATIVE false
#endif

#ifdef CONFIG_PYLON_LOG_FRAMES
#define LOG_FRAMES true
#else
#define LOG_FRAMES false
#endif

static const pylon_cfg_t s_cfg = {
    .adr = CONFIG_PYLON_ADR,
    .sys_adr = CONFIG_PYLON_SYS_ADR,
    .proto_ver = CONFIG_PYLON_PROTO_VER,
    .current_unit_ma = CURRENT_UNIT_MA,
    .dcl_negative = DCL_NEGATIVE,
    .device_name = CONFIG_PYLON_DEVICE_NAME,
    .manufacturer = CONFIG_PYLON_MANUFACTURER,
    .sw_version = 0x0100,
    .serial = CONFIG_PYLON_SERIAL,
};

static limits_cfg_t s_lim_cfg;
static pylon_rs485_stats_t s_stats;
static uint32_t s_cid2_count[256];
static portMUX_TYPE s_stats_lock = portMUX_INITIALIZER_UNLOCKED;

static void log_frame(const char *dir, const char *buf, size_t len)
{
    /* Без завершающего '\r', чтобы строка лога не ломалась. */
    ESP_LOGI(TAG, "%s %.*s", dir, (int)(len ? len - 1 : 0), buf);
}

static void handle_frame(const char *req, size_t len)
{
    static char resp[RESP_MAX];
    int64_t now = esp_timer_get_time();

    pack_summary_t pack;
    limits_out_t lim;
    pack_store_summary(now, &pack);
    limits_compute(&s_lim_cfg, &pack, &lim);

    if (LOG_FRAMES) {
        log_frame("<-", req, len);
    }

    pylon_data_t data = {.pack = &pack, .lim = &lim, .lim_cfg = &s_lim_cfg};
    pylon_req_info_t info;
    size_t n = pylon_handle_request(&s_cfg, &data, req, len, resp, sizeof(resp), &info);

    bool first = false;
    bool silent = false;
    if (info.parse_rc != PYLON_PARSE_NOT_FRAME && info.for_us) {
        /* ТЗ 5.2: без данных BMZ дольше таймаута молчим, чтобы инвертор
         * показал ошибку связи. После старта отсчёт идёт от загрузки. */
        silent = now - pack.newest_us > SILENCE_US;
    }

    taskENTER_CRITICAL(&s_stats_lock);
    if (info.parse_rc == PYLON_PARSE_NOT_FRAME) {
        s_stats.garbage++;
    } else {
        s_stats.frames++;
        if (info.for_us) {
            s_stats.for_us++;
            s_stats.last_request_us = now;
            first = s_cid2_count[info.cid2]++ == 0;
            if (silent) {
                s_stats.silenced++;
            } else if (n) {
                s_stats.responses++;
                if (info.rtn != PYLON_RTN_OK) {
                    s_stats.errors++;
                }
            }
        }
    }
    taskEXIT_CRITICAL(&s_stats_lock);

    if (first) {
        ESP_LOGI(TAG, "инвертор впервые запросил CID2 0x%02X (VER 0x%02X, ADR 0x%02X)%s",
                 info.cid2, info.ver, info.adr,
                 info.rtn == PYLON_RTN_CID2 ? " — не поддерживается, ответ RTN 0x04" : "");
    }
    if (info.parse_rc > 0 && info.for_us) {
        ESP_LOGW(TAG, "кадр с ошибкой RTN 0x%02X: %.*s", info.parse_rc, (int)len - 1, req);
    }
    if (silent || n == 0) {
        return;
    }
    uart_write_bytes(UART_PORT, resp, n);
    if (LOG_FRAMES) {
        log_frame("->", resp, n);
    }
}

static void rs485_task(void *arg)
{
    (void)arg;
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    static char frame[REQ_MAX];
    size_t flen = 0;
    bool in_frame = false;
    bool poll_lost_logged = false;
    uint8_t rx[64];

    for (;;) {
        esp_task_wdt_reset();
        int n = uart_read_bytes(UART_PORT, rx, sizeof(rx), pdMS_TO_TICKS(50));
        for (int i = 0; i < n; i++) {
            char c = (char)rx[i];
            if (c == PYLON_SOI) {
                in_frame = true;
                flen = 0;
            }
            if (!in_frame) {
                continue;
            }
            if (flen >= sizeof(frame)) {
                in_frame = false;
                taskENTER_CRITICAL(&s_stats_lock);
                s_stats.garbage++;
                taskEXIT_CRITICAL(&s_stats_lock);
                continue;
            }
            frame[flen++] = c;
            if (c == PYLON_EOI) {
                in_frame = false;
                handle_frame(frame, flen);
            }
        }

        int64_t now = esp_timer_get_time();
        taskENTER_CRITICAL(&s_stats_lock);
        int64_t last = s_stats.last_request_us;
        taskEXIT_CRITICAL(&s_stats_lock);
        if (last && now - last > NO_POLL_US) {
            if (!poll_lost_logged) {
                ESP_LOGW(TAG, "инвертор не опрашивает батарею больше %lld с", NO_POLL_US / 1000000);
                poll_lost_logged = true;
            }
        } else if (last && poll_lost_logged) {
            ESP_LOGI(TAG, "инвертор снова опрашивает батарею");
            poll_lost_logged = false;
        }
    }
}

esp_err_t pylon_rs485_start(const limits_cfg_t *lim_cfg)
{
    s_lim_cfg = *lim_cfg;

    const uart_config_t uc = {
        .baud_rate = CONFIG_PYLON_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t err = uart_driver_install(UART_PORT, 512, 512, 0, NULL, 0);
    if (err == ESP_OK) {
        err = uart_param_config(UART_PORT, &uc);
    }
    if (err == ESP_OK) {
        err = uart_set_pin(UART_PORT, CONFIG_PYLON_TX_GPIO, CONFIG_PYLON_RX_GPIO,
                           CONFIG_PYLON_DE_GPIO >= 0 ? CONFIG_PYLON_DE_GPIO : UART_PIN_NO_CHANGE,
                           UART_PIN_NO_CHANGE);
    }
    if (err == ESP_OK && CONFIG_PYLON_DE_GPIO >= 0) {
        err = uart_set_mode(UART_PORT, UART_MODE_RS485_HALF_DUPLEX);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART%d не запустился: %s", CONFIG_PYLON_UART_NUM, esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "UART%d %d бод, TX=GPIO%d RX=GPIO%d DE=%d, ADR 0x%02X / 0x%02X",
             CONFIG_PYLON_UART_NUM, CONFIG_PYLON_BAUD, CONFIG_PYLON_TX_GPIO,
             CONFIG_PYLON_RX_GPIO, CONFIG_PYLON_DE_GPIO, CONFIG_PYLON_ADR, CONFIG_PYLON_SYS_ADR);

    /* Приоритет выше драйвера BMZ: ответ инвертору нужен за 100 мс (ТЗ F3). */
    if (xTaskCreate(rs485_task, "pylon485", 4096, NULL, 12, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void pylon_rs485_get_stats(pylon_rs485_stats_t *out)
{
    taskENTER_CRITICAL(&s_stats_lock);
    *out = s_stats;
    taskEXIT_CRITICAL(&s_stats_lock);
}

void pylon_rs485_print_cid2(void)
{
    printf("CID2, которые запрашивал инвертор:\n");
    bool any = false;
    for (int i = 0; i < 256; i++) {
        uint32_t c = s_cid2_count[i];
        if (c) {
            printf("  0x%02X: %" PRIu32 "\n", i, c);
            any = true;
        }
    }
    if (!any) {
        printf("  запросов не было\n");
    }
}
