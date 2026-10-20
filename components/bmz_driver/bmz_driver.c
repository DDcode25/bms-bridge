#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "bmz_can.h"
#include "bmz_driver.h"
#include "pack_model.h"

static const char *TAG = "bmz";

#define STALE_US ((int64_t)CONFIG_BMZ_STALE_S * 1000000)

uint8_t bmz_driver_module_count(void)
{
#ifdef CONFIG_BMZ_SOURCE_SIM
    return CONFIG_BMZ_MODULES;
#else
    return 1;   /* гипотеза А: весь стек — один источник на шине */
#endif
}

/* ---------------- симулятор ---------------- */

#ifdef CONFIG_BMZ_SOURCE_SIM

typedef struct {
    uint16_t cell_mv[PACK_SERIES_CELLS];
    int16_t temp_dc;
    int32_t current_ma;   /* на всю батарею */
    uint8_t soc;
    bool paused;
    bool online[PACK_MAX_MODULES];
} sim_state_t;

static sim_state_t s_sim;
static portMUX_TYPE s_sim_lock = portMUX_INITIALIZER_UNLOCKED;

const char *bmz_driver_source_name(void) { return "симулятор"; }

void bmz_sim_set_cells(uint16_t mv)
{
    taskENTER_CRITICAL(&s_sim_lock);
    for (int i = 0; i < PACK_SERIES_CELLS; i++) {
        s_sim.cell_mv[i] = mv;
    }
    taskEXIT_CRITICAL(&s_sim_lock);
}

void bmz_sim_set_cell(uint8_t idx, uint16_t mv)
{
    if (idx >= PACK_SERIES_CELLS) {
        return;
    }
    taskENTER_CRITICAL(&s_sim_lock);
    s_sim.cell_mv[idx] = mv;
    taskEXIT_CRITICAL(&s_sim_lock);
}

void bmz_sim_set_temp(int16_t dc)
{
    taskENTER_CRITICAL(&s_sim_lock);
    s_sim.temp_dc = dc;
    taskEXIT_CRITICAL(&s_sim_lock);
}

void bmz_sim_set_current(int32_t ma)
{
    taskENTER_CRITICAL(&s_sim_lock);
    s_sim.current_ma = ma;
    taskEXIT_CRITICAL(&s_sim_lock);
}

void bmz_sim_set_soc(uint8_t pct)
{
    taskENTER_CRITICAL(&s_sim_lock);
    s_sim.soc = pct > 100 ? 100 : pct;
    taskEXIT_CRITICAL(&s_sim_lock);
}

void bmz_sim_set_paused(bool paused)
{
    taskENTER_CRITICAL(&s_sim_lock);
    s_sim.paused = paused;
    taskEXIT_CRITICAL(&s_sim_lock);
}

void bmz_sim_set_module_online(uint8_t idx, bool online)
{
    if (idx >= PACK_MAX_MODULES) {
        return;
    }
    taskENTER_CRITICAL(&s_sim_lock);
    s_sim.online[idx] = online;
    taskEXIT_CRITICAL(&s_sim_lock);
}

static void sim_task(void *arg)
{
    (void)arg;
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    const uint8_t n = CONFIG_BMZ_MODULES;
    for (;;) {
        esp_task_wdt_reset();
        sim_state_t st;
        taskENTER_CRITICAL(&s_sim_lock);
        st = s_sim;
        taskEXIT_CRITICAL(&s_sim_lock);

        if (!st.paused) {
            int64_t now = esp_timer_get_time();
            uint8_t online = 0;
            for (uint8_t i = 0; i < n; i++) {
                online += st.online[i];
            }
            for (uint8_t i = 0; i < n; i++) {
                if (!st.online[i]) {
                    continue;   /* данные не обновляются — модуль «выпал» */
                }
                pack_module_t m = {0};
                m.updated_us = now;
                m.units = 1;
                m.n_cells = PACK_SERIES_CELLS;
                for (int c = 0; c < PACK_SERIES_CELLS; c++) {
                    m.cell_mv[c] = st.cell_mv[c];
                    m.voltage_mv += st.cell_mv[c];
                }
                m.current_ma = online ? st.current_ma / online : 0;
                m.soc_pct = st.soc;
                m.soh_pct = 100;
                m.cycles = 10;
                m.capacity_mah = CONFIG_BMZ_UNIT_CAPACITY_MAH;
                m.n_temps = 4;
                for (int t = 0; t < 4; t++) {
                    m.temp_dc[t] = st.temp_dc;
                }
                m.bms_charge_allowed = true;
                m.bms_discharge_allowed = true;
                pack_store_put(i, &m);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

esp_err_t bmz_driver_start(void)
{
    bmz_sim_set_cells(CONFIG_BMZ_SIM_CELL_MV);
    bmz_sim_set_temp(CONFIG_BMZ_SIM_TEMP_DC);
    bmz_sim_set_soc(CONFIG_BMZ_SIM_SOC);
    for (int i = 0; i < PACK_MAX_MODULES; i++) {
        bmz_sim_set_module_online(i, true);
    }
    ESP_LOGW(TAG, "СИМУЛЯТОР: %d модул(ь/я), ячейка %d мВ, %d.%d °C, SOC %d %%",
             CONFIG_BMZ_MODULES, CONFIG_BMZ_SIM_CELL_MV, CONFIG_BMZ_SIM_TEMP_DC / 10,
             CONFIG_BMZ_SIM_TEMP_DC % 10, CONFIG_BMZ_SIM_SOC);
    return xTaskCreate(sim_task, "bmz_sim", 3072, NULL, 10, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

#else /* !CONFIG_BMZ_SOURCE_SIM */

void bmz_sim_set_cells(uint16_t mv) { (void)mv; }
void bmz_sim_set_cell(uint8_t idx, uint16_t mv) { (void)idx; (void)mv; }
void bmz_sim_set_temp(int16_t dc) { (void)dc; }
void bmz_sim_set_current(int32_t ma) { (void)ma; }
void bmz_sim_set_soc(uint8_t pct) { (void)pct; }
void bmz_sim_set_paused(bool paused) { (void)paused; }
void bmz_sim_set_module_online(uint8_t idx, bool online) { (void)idx; (void)online; }

#endif

/* ---------------- CAN, гипотеза А ---------------- */

#ifdef CONFIG_BMZ_SOURCE_CAN_A

#include "driver/twai.h"

const char *bmz_driver_source_name(void) { return "CAN (гипотеза А)"; }

static void can_rx_task(void *arg)
{
    (void)arg;
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    bmz_can_state_t st = {0};
    twai_message_t msg;
    int64_t last_keepalive = 0;

    for (;;) {
        esp_task_wdt_reset();
        if (twai_receive(&msg, pdMS_TO_TICKS(100)) == ESP_OK && !msg.extd && !msg.rtr) {
            int64_t now = esp_timer_get_time();
            if (bmz_can_feed(&st, msg.identifier, msg.data, msg.data_length_code, now)) {
                pack_module_t m;
                bmz_can_to_module(&st, now, STALE_US, CONFIG_BMZ_MODULES, &m);
                pack_store_put(0, &m);
            }
        }

        uint32_t alerts = 0;
        if (twai_read_alerts(&alerts, 0) == ESP_OK) {
            if (alerts & TWAI_ALERT_BUS_OFF) {
                ESP_LOGE(TAG, "CAN BUS-OFF, восстановление");
                twai_initiate_recovery();
            }
            if (alerts & TWAI_ALERT_BUS_RECOVERED) {
                ESP_LOGI(TAG, "CAN восстановлен");
                twai_start();
            }
            if (alerts & TWAI_ALERT_RX_QUEUE_FULL) {
                ESP_LOGW(TAG, "очередь CAN переполнена");
            }
        }

#ifdef CONFIG_BMZ_KEEPALIVE
        int64_t now = esp_timer_get_time();
        if (now - last_keepalive >= (int64_t)CONFIG_BMZ_KEEPALIVE_PERIOD_MS * 1000) {
            twai_message_t ka = {.identifier = CONFIG_BMZ_KEEPALIVE_ID, .data_length_code = 8};
            twai_transmit(&ka, 0);
            last_keepalive = now;
        }
#else
        (void)last_keepalive;
#endif
    }
}

esp_err_t bmz_driver_start(void)
{
    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(CONFIG_BMZ_CAN_TX_GPIO,
                                                          CONFIG_BMZ_CAN_RX_GPIO,
                                                          TWAI_MODE_NORMAL);
    g.rx_queue_len = 32;
    g.alerts_enabled = TWAI_ALERT_BUS_OFF | TWAI_ALERT_BUS_RECOVERED | TWAI_ALERT_RX_QUEUE_FULL;
#ifdef CONFIG_BMZ_CAN_250K
    twai_timing_config_t t = TWAI_TIMING_CONFIG_250KBITS();
#else
    twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
#endif
    twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    esp_err_t err = twai_driver_install(&g, &t, &f);
    if (err == ESP_OK) {
        err = twai_start();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TWAI не запустился: %s", esp_err_to_name(err));
        return err;
    }
    /* NORMAL-режим: шлюз подтверждает (ACK) кадры модуля, иначе одиночный
     * модуль повторяет кадр с ошибкой (см. tools/bmz_can_sniffer). */
    ESP_LOGI(TAG, "CAN гипотеза А: TX=GPIO%d RX=GPIO%d, keep-alive %s",
             CONFIG_BMZ_CAN_TX_GPIO, CONFIG_BMZ_CAN_RX_GPIO,
#ifdef CONFIG_BMZ_KEEPALIVE
             "ВКЛ"
#else
             "выкл"
#endif
    );
    return xTaskCreate(can_rx_task, "bmz_can", 4096, NULL, 10, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

#endif
