/*
 * BMZ CAN sniffer / prober for ESP32 (ESP-IDF v5.x, legacy TWAI driver)
 *
 * - Работает в NORMAL-режиме: ESP32 подтверждает (ACK) кадры модуля.
 *   В режиме listen-only одиночный модуль не получает ACK, его кадры
 *   обрываются error-кадром и сниффер ничего не видит.
 * - Автоопределение скорости: 500k -> 250k -> 125k -> 1M.
 * - Печать сырых кадров + расшифровка семейства 0x351..0x379
 *   (Pylontech / "Victron CAN-bus BMS" профиль, который использует BMZ ESS).
 * - Keep-alive мастера (по умолчанию ID 0x305, 8 нулевых байт, раз в 1 с)
 *   включается/выключается кнопкой BOOT (GPIO0). По умолчанию ВЫКЛЮЧЕН.
 * - Сводка по ID и состоянию шины каждые 5 с.
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/twai.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

static const char *TAG = "bmz";

#define TX_GPIO   CONFIG_BMZ_CAN_TX_GPIO
#define RX_GPIO   CONFIG_BMZ_CAN_RX_GPIO
#define BTN_GPIO  CONFIG_BMZ_KEEPALIVE_BUTTON_GPIO

#ifdef CONFIG_BMZ_KEEPALIVE_AT_START
#define KA_AT_START true
#else
#define KA_AT_START false
#endif

#ifdef CONFIG_BMZ_AUTO_BAUD
#define AUTO_BAUD true
#else
#define AUTO_BAUD false
#endif

#ifdef CONFIG_BMZ_PRINT_RAW
#define PRINT_RAW true
#else
#define PRINT_RAW false
#endif

typedef struct {
    const char *name;
    twai_timing_config_t timing;
} rate_t;

/* Индексы: 0=125k 1=250k 2=500k 3=1M */
static const rate_t RATES[] = {
    {"125k", TWAI_TIMING_CONFIG_125KBITS()},
    {"250k", TWAI_TIMING_CONFIG_250KBITS()},
    {"500k", TWAI_TIMING_CONFIG_500KBITS()},
    {"1M",   TWAI_TIMING_CONFIG_1MBITS()},
};

#if defined(CONFIG_BMZ_BITRATE_125K)
#define DEFAULT_RATE 0
#elif defined(CONFIG_BMZ_BITRATE_250K)
#define DEFAULT_RATE 1
#elif defined(CONFIG_BMZ_BITRATE_1M)
#define DEFAULT_RATE 3
#else
#define DEFAULT_RATE 2
#endif

static volatile bool s_keepalive = KA_AT_START;

/* ---------- helpers ---------- */

static uint16_t u16le(const uint8_t *d) { return (uint16_t)(d[0] | (d[1] << 8)); }
static int16_t  s16le(const uint8_t *d) { return (int16_t)(d[0] | (d[1] << 8)); }

static bool install(int idx, twai_mode_t mode)
{
    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(TX_GPIO, RX_GPIO, mode);
    g.rx_queue_len = 64;
    g.tx_queue_len = 4;
    g.alerts_enabled = TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_OFF |
                       TWAI_ALERT_BUS_RECOVERED | TWAI_ALERT_RX_QUEUE_FULL |
                       TWAI_ALERT_TX_FAILED;
    twai_timing_config_t t = RATES[idx].timing;
    twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    if (twai_driver_install(&g, &t, &f) != ESP_OK) {
        ESP_LOGE(TAG, "twai_driver_install(%s) не удалась", RATES[idx].name);
        return false;
    }
    if (twai_start() != ESP_OK) {
        twai_driver_uninstall();
        return false;
    }
    return true;
}

static void uninstall(void)
{
    twai_stop();             /* может вернуть ошибку в bus-off — это нормально */
    twai_driver_uninstall();
}

/* ---------- автоопределение скорости ---------- */

static int scan_bitrate(void)
{
    static const int order[] = {2, 1, 0, 3};
    for (size_t i = 0; i < sizeof(order) / sizeof(order[0]); i++) {
        int idx = order[i];
        if (!install(idx, TWAI_MODE_NORMAL)) {
            continue;
        }
        int frames = 0;
        twai_message_t m;
        int64_t end = esp_timer_get_time() + 2500000;
        while (esp_timer_get_time() < end) {
            if (twai_receive(&m, pdMS_TO_TICKS(100)) == ESP_OK) {
                frames++;
            }
        }
        twai_status_info_t s = {0};
        twai_get_status_info(&s);
        ESP_LOGI(TAG, "скорость %-4s: кадров=%d, ошибок шины=%" PRIu32 ", rx_err=%" PRIu32,
                 RATES[idx].name, frames, s.bus_error_count, s.rx_error_counter);
        uninstall();
        if (frames >= 3 && s.bus_error_count < (uint32_t)frames) {
            return idx;
        }
    }
    return -1;
}

/* ---------- статистика по ID ---------- */

typedef struct {
    uint32_t id;
    bool extd;
    uint32_t count;
    uint8_t dlc;
    uint8_t last[8];
} stat_t;

#define MAX_IDS 64
static stat_t s_stats[MAX_IDS];
static int s_nstats;

static void stats_add(const twai_message_t *m)
{
    for (int i = 0; i < s_nstats; i++) {
        if (s_stats[i].id == m->identifier && s_stats[i].extd == (bool)m->extd) {
            s_stats[i].count++;
            s_stats[i].dlc = m->data_length_code;
            memcpy(s_stats[i].last, m->data, 8);
            return;
        }
    }
    if (s_nstats < MAX_IDS) {
        stat_t *s = &s_stats[s_nstats++];
        s->id = m->identifier;
        s->extd = m->extd;
        s->count = 1;
        s->dlc = m->data_length_code;
        memcpy(s->last, m->data, 8);
        ESP_LOGW(TAG, "новый ID: 0x%0*" PRIX32 " (%s)", m->extd ? 8 : 3,
                 m->identifier, m->extd ? "29 бит" : "11 бит");
    }
}

static void print_summary(double seconds)
{
    twai_status_info_t s = {0};
    twai_get_status_info(&s);
    const char *state = s.state == TWAI_STATE_RUNNING ? "RUNNING" :
                        s.state == TWAI_STATE_BUS_OFF ? "BUS_OFF" :
                        s.state == TWAI_STATE_RECOVERING ? "RECOVERING" : "STOPPED";
    printf("\n===== сводка за %.1f с | шина %s | rx_err=%" PRIu32 " tx_err=%" PRIu32
           " bus_err=%" PRIu32 " | keep-alive %s =====\n",
           seconds, state, s.rx_error_counter, s.tx_error_counter, s.bus_error_count,
           s_keepalive ? "ВКЛ" : "выкл");
    if (s_nstats == 0) {
        printf("  кадров нет\n");
    }
    for (int i = 0; i < s_nstats; i++) {
        stat_t *st = &s_stats[i];
        printf("  %s 0x%0*" PRIX32 "  %3" PRIu32 " шт (%.1f/с)  последний:",
               st->extd ? "E" : "S", st->extd ? 8 : 3, st->id, st->count,
               st->count / seconds);
        for (int b = 0; b < st->dlc && b < 8; b++) {
            printf(" %02X", st->last[b]);
        }
        printf("\n");
        st->count = 0;
    }
    printf("\n");
}

/* ---------- печать и расшифровка ---------- */

static void print_raw(const twai_message_t *m)
{
    printf("%10lld  %s 0x%0*" PRIX32 "  [%d]", esp_timer_get_time() / 1000,
           m->extd ? "E" : "S", m->extd ? 8 : 3, m->identifier, m->data_length_code);
    for (int i = 0; i < m->data_length_code && i < 8; i++) {
        printf(" %02X", m->data[i]);
    }
    if (m->rtr) {
        printf("  RTR");
    }
    printf("\n");
}

static void print_ascii(const char *label, const twai_message_t *m)
{
    char s[9] = {0};
    for (int i = 0; i < m->data_length_code && i < 8; i++) {
        uint8_t c = m->data[i];
        s[i] = (c >= 0x20 && c < 0x7F) ? (char)c : '.';
    }
    printf("      -> %s: \"%s\"\n", label, s);
}

/* Расшифровка по Pylontech LV CAN V1.3 / профилю "CAN-bus BMS LV".
 * Для BMZ это гипотеза: проверить по мультиметру и токовым клещам. */
static void decode(const twai_message_t *m)
{
    if (m->extd || m->rtr) {
        return;
    }
    const uint8_t *d = m->data;
    int n = m->data_length_code;
    switch (m->identifier) {
    case 0x351:
        if (n >= 6) {
            printf("      -> лимиты: CVL=%.1f В  CCL=%.1f А  DCL=%.1f А",
                   u16le(d) / 10.0, s16le(d + 2) / 10.0, s16le(d + 4) / 10.0);
            if (n >= 8) {
                printf("  DVL=%.1f В", u16le(d + 6) / 10.0);
            }
            printf("\n");
        }
        break;
    case 0x355:
        if (n >= 4) {
            printf("      -> SOC=%u %%  SOH=%u %%", u16le(d), u16le(d + 2));
            if (n >= 6) {
                printf("  SOC(0.01)=%.2f %%", u16le(d + 4) / 100.0);
            }
            printf("\n");
        }
        break;
    case 0x356:
        if (n >= 6) {
            printf("      -> U=%.2f В  I=%.1f А  T=%.1f °C\n",
                   s16le(d) / 100.0, s16le(d + 2) / 10.0, s16le(d + 4) / 10.0);
        }
        break;
    case 0x359:
        if (n >= 5) {
            printf("      -> защиты %02X %02X, аварии %02X %02X, модулей=%u",
                   d[0], d[1], d[2], d[3], d[4]);
            if (n >= 7) {
                printf(", метка '%c%c'", d[5] >= 0x20 ? d[5] : '.', d[6] >= 0x20 ? d[6] : '.');
            }
            printf("\n");
        }
        break;
    case 0x35A:
        printf("      -> аварии/предупреждения (Victron 0x35A):");
        for (int i = 0; i < n; i++) {
            printf(" %02X", d[i]);
        }
        printf("\n");
        break;
    case 0x35C:
        if (n >= 1) {
            printf("      -> флаги: заряд %s, разряд %s, force1 %d, force2 %d, full %d\n",
                   (d[0] & 0x80) ? "разрешён" : "ЗАПРЕЩЁН",
                   (d[0] & 0x40) ? "разрешён" : "ЗАПРЕЩЁН",
                   !!(d[0] & 0x20), !!(d[0] & 0x10), !!(d[0] & 0x08));
        }
        break;
    case 0x35E:
        print_ascii("производитель", m);
        break;
    case 0x370:
    case 0x371:
        print_ascii("имя BMS", m);
        break;
    case 0x35F:
        if (n >= 6) {
            printf("      -> модель=0x%04X, прошивка=0x%04X, ёмкость онлайн=%u А·ч\n",
                   u16le(d), u16le(d + 2), u16le(d + 4));
        }
        break;
    case 0x372:
        /* diyBMS шлёт только первое поле (2 байта), полный формат — 8 байт */
        if (n >= 2) {
            printf("      -> модулей в работе=%u", u16le(d));
            if (n >= 8) {
                printf(", блок заряда=%u, блок разряда=%u, offline=%u",
                       u16le(d + 2), u16le(d + 4), u16le(d + 6));
            }
            printf("\n");
        }
        break;
    case 0x374:
        print_ascii("ячейка с min U", m);
        break;
    case 0x375:
        print_ascii("ячейка с max U", m);
        break;
    case 0x376:
        print_ascii("датчик с min T", m);
        break;
    case 0x377:
        print_ascii("датчик с max T", m);
        break;
    case 0x373:
        if (n >= 8) {
            printf("      -> ячейки: min=%u мВ max=%u мВ (разбаланс %d мВ), T min=%.1f max=%.1f °C\n",
                   u16le(d), u16le(d + 2), (int)u16le(d + 2) - (int)u16le(d),
                   u16le(d + 4) - 273.15, u16le(d + 6) - 273.15);
        }
        break;
    case 0x379:
        if (n >= 2) {
            printf("      -> установленная ёмкость=%u А·ч\n", u16le(d));
        }
        break;
    default:
        break;
    }
}

/* ---------- задачи ---------- */

static void rx_task(void *arg)
{
    (void)arg;
    int64_t last = esp_timer_get_time();
    twai_message_t m;
    for (;;) {
        if (twai_receive(&m, pdMS_TO_TICKS(500)) == ESP_OK) {
            stats_add(&m);
            if (PRINT_RAW) {
                print_raw(&m);
            }
            decode(&m);
        }
        int64_t now = esp_timer_get_time();
        if (now - last >= 5000000) {
            print_summary((now - last) / 1e6);
            last = now;
        }
    }
}

static void alert_task(void *arg)
{
    (void)arg;
    int64_t last_tx_warn = 0;
    for (;;) {
        uint32_t a = 0;
        if (twai_read_alerts(&a, portMAX_DELAY) != ESP_OK) {
            continue;
        }
        if (a & TWAI_ALERT_ERR_PASS) {
            ESP_LOGW(TAG, "контроллер в error-passive (много ошибок на шине)");
        }
        if (a & TWAI_ALERT_RX_QUEUE_FULL) {
            ESP_LOGW(TAG, "очередь приёма переполнена, часть кадров потеряна");
        }
        if (a & TWAI_ALERT_TX_FAILED) {
            int64_t now = esp_timer_get_time();
            if (now - last_tx_warn > 10000000) {
                ESP_LOGW(TAG, "keep-alive не подтверждён (нет ACK от модуля?)");
                last_tx_warn = now;
            }
        }
        if (a & TWAI_ALERT_BUS_OFF) {
            ESP_LOGE(TAG, "BUS-OFF, запускаю восстановление");
            twai_initiate_recovery();
        }
        if (a & TWAI_ALERT_BUS_RECOVERED) {
            ESP_LOGI(TAG, "шина восстановлена");
            twai_start();
        }
    }
}

static void keepalive_task(void *arg)
{
    (void)arg;
    twai_message_t m = {0};
    m.identifier = CONFIG_BMZ_KEEPALIVE_ID;
    m.data_length_code = 8;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(CONFIG_BMZ_KEEPALIVE_PERIOD_MS));
        if (s_keepalive) {
            twai_transmit(&m, 0);
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "BMZ CAN sniffer: TX=GPIO%d RX=GPIO%d, кнопка keep-alive GPIO%d",
             TX_GPIO, RX_GPIO, BTN_GPIO);

    gpio_config_t btn = {
        .pin_bit_mask = 1ULL << BTN_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn);

    int idx = -1;
    if (AUTO_BAUD) {
        ESP_LOGI(TAG, "автоопределение скорости...");
        idx = scan_bitrate();
    }
    if (idx < 0) {
        idx = DEFAULT_RATE;
        ESP_LOGW(TAG, "трафик не найден, слушаю на %s. Если модуль ждёт мастера, "
                      "нажмите BOOT, чтобы включить keep-alive 0x%03X.",
                 RATES[idx].name, CONFIG_BMZ_KEEPALIVE_ID);
    } else {
        ESP_LOGI(TAG, "найдена скорость %s", RATES[idx].name);
    }

    if (!install(idx, TWAI_MODE_NORMAL)) {
        ESP_LOGE(TAG, "не удалось запустить TWAI, проверьте GPIO");
        return;
    }

    xTaskCreate(rx_task, "rx", 4096, NULL, 10, NULL);
    xTaskCreate(alert_task, "alerts", 3072, NULL, 11, NULL);
    xTaskCreate(keepalive_task, "keepalive", 2048, NULL, 9, NULL);

    int prev = 1;
    for (;;) {
        int lvl = gpio_get_level(BTN_GPIO);
        if (prev == 1 && lvl == 0) {
            s_keepalive = !s_keepalive;
            ESP_LOGW(TAG, "keep-alive 0x%03X каждые %d мс: %s", CONFIG_BMZ_KEEPALIVE_ID,
                     CONFIG_BMZ_KEEPALIVE_PERIOD_MS, s_keepalive ? "ВКЛЮЧЁН" : "выключен");
        }
        prev = lvl;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
