/*
 * bms-bridge: шлюз BMZ 618031 → инвертор (Pylontech RS485).
 * ТЗ: docs/TZ.md.
 */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "app.h"
#include "bmz_driver.h"
#include "limits_calc.h"
#include "pack_model.h"
#include "pylon_rs485.h"

static const char *TAG = "bridge";

#define STATUS_PERIOD_MS 10000

static const limits_cfg_t s_lim_cfg = {
    .cvl_mv = CONFIG_LIM_CVL_MV,
    .dvl_mv = CONFIG_LIM_DVL_MV,
    .unit_current_da = CONFIG_LIM_UNIT_CURRENT_DA,
    .unit_taper_current_da = CONFIG_LIM_UNIT_TAPER_CURRENT_DA,
    .cell_ov_mv = CONFIG_LIM_CELL_OV_MV,
    .cell_taper_mv = CONFIG_LIM_CELL_TAPER_MV,
    .cell_uv_mv = CONFIG_LIM_CELL_UV_MV,
    .imbalance_mv = CONFIG_LIM_IMBALANCE_MV,
    .imbalance_upper_mv = CONFIG_LIM_IMBALANCE_UPPER_MV,
    .charge_temp_min_dc = CONFIG_LIM_CHARGE_TEMP_MIN_DC,
    .temp_derate_dc = CONFIG_LIM_TEMP_DERATE_DC,
    .temp_cutoff_dc = CONFIG_LIM_TEMP_CUTOFF_DC,
    .force_charge_soc = CONFIG_LIM_FORCE_CHARGE_SOC,
};

const limits_cfg_t *app_limits_cfg(void) { return &s_lim_cfg; }

static void fmt_dc(char *buf, size_t cap, int16_t dc)
{
    snprintf(buf, cap, "%s%d.%d", dc < 0 ? "-" : "", abs(dc) / 10, abs(dc) % 10);
}

void app_print_status(void)
{
    int64_t now = esp_timer_get_time();
    pack_summary_t p;
    limits_out_t l;
    pack_store_summary(now, &p);
    limits_compute(&s_lim_cfg, &p, &l);

    char reasons[160], tmin[8], tmax[8];
    limits_reasons_str(l.reasons, reasons, sizeof(reasons));
    fmt_dc(tmin, sizeof(tmin), p.temp_min_dc);
    fmt_dc(tmax, sizeof(tmax), p.temp_max_dc);

    pylon_rs485_stats_t st;
    pylon_rs485_get_stats(&st);
    long long poll_age = st.last_request_us ? (now - st.last_request_us) / 1000000 : -1;
    long long data_age = p.newest_us ? (now - p.newest_us) / 1000000 : -1;

    printf("[%s] модули %u/%u (физ. %u), данные %llds назад | %" PRIu32 " мВ %" PRId32 " мА SOC %s%u%%\n"
           "  ячейки %u…%u мВ%s | T %s…%s °C\n"
           "  CVL %u DVL %u мВ | CCL %u.%u A DCL %u.%u A | заряд %s разряд %s%s | %s\n"
           "  инвертор: запросов %" PRIu32 ", ответов %" PRIu32 ", ошибок %" PRIu32
           ", без ответа %" PRIu32 ", последний %llds назад\n",
           bmz_driver_source_name(), p.modules_ok, p.modules_cfg, p.units_ok, data_age,
           p.voltage_mv, p.current_ma, p.soc_valid ? "" : "?", p.soc_pct,
           p.cell_min_mv, p.cell_max_mv, p.cells_synthetic ? " (только min/max)" : "", tmin, tmax,
           l.cvl_mv, l.dvl_mv, l.ccl_da / 10, l.ccl_da % 10, l.dcl_da / 10, l.dcl_da % 10,
           l.charge_enable ? "да" : "НЕТ", l.discharge_enable ? "да" : "НЕТ",
           l.charge_immediately ? ", ЗАРЯДИТЬ" : "", reasons,
           st.for_us, st.responses, st.errors, st.silenced, poll_age);
}

void app_main(void)
{
    ESP_LOGI(TAG, "bms-bridge, источник: %s", bmz_driver_source_name());

    const pack_cfg_t pack_cfg = {
        .stale_us = (int64_t)CONFIG_BMZ_STALE_S * 1000000,
        .unit_capacity_mah = CONFIG_BMZ_UNIT_CAPACITY_MAH,
    };
    pack_store_init(bmz_driver_module_count(), &pack_cfg);

    ESP_ERROR_CHECK(bmz_driver_start());
    ESP_ERROR_CHECK(pylon_rs485_start(&s_lim_cfg));
    console_start();

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(STATUS_PERIOD_MS));
        app_print_status();
    }
}
