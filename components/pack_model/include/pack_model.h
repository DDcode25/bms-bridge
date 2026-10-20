/*
 * pack_model — данные по модулям BMZ и сводка по всей батарее (ТЗ 4.3, 6.1).
 *
 * Единицы внутри прошивки (все целые, без float):
 *   напряжение — мВ, ток — мА (заряд «+»), температура — 0,1 °C,
 *   ёмкость — мА·ч, время — мкс по esp_timer_get_time().
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PACK_SERIES_CELLS 16   /* LFP 16S, этикетка BMZ 618031 */
#define PACK_MAX_TEMPS    8
#define PACK_MAX_MODULES  8    /* ТЗ F2: от 1 до 8 модулей */

#define PACK_SOC_UNKNOWN  0xFF

/* Данные одного источника. Источник — это модуль BMZ или, если BMZ сам
 * сводит стек (гипотеза А, профиль Victron), весь стек: тогда units > 1. */
typedef struct {
    int64_t updated_us;        /* время последних свежих данных; 0 — данных не было */
    uint8_t units;             /* сколько физических модулей за этим источником (0 → 1) */

    uint32_t voltage_mv;
    int32_t current_ma;        /* заряд «+» */
    uint8_t soc_pct;           /* PACK_SOC_UNKNOWN, если BMZ не отдаёт */
    uint8_t soh_pct;           /* 0 — неизвестно */
    uint16_t cycles;
    uint32_t capacity_mah;     /* полная ёмкость; 0 — взять из настроек */

    uint8_t n_cells;           /* 0 — известны только min/max ниже */
    uint16_t cell_mv[PACK_SERIES_CELLS];
    uint16_t cell_min_mv;      /* используются, если n_cells == 0; 0 — неизвестно */
    uint16_t cell_max_mv;

    uint8_t n_temps;
    int16_t temp_dc[PACK_MAX_TEMPS];

    /* Лимиты и запреты, которые прислала сама BMS (если прислала). */
    bool bms_limits_valid;
    uint16_t bms_cvl_mv;
    uint16_t bms_dvl_mv;
    uint16_t bms_ccl_da;       /* 0,1 А */
    uint16_t bms_dcl_da;
    bool bms_charge_allowed;
    bool bms_discharge_allowed;
} pack_module_t;

typedef struct {
    int64_t stale_us;           /* источник без данных дольше — исключается (ТЗ 5.2: 10 с) */
    uint32_t unit_capacity_mah; /* ёмкость модуля, если BMZ её не отдаёт */
} pack_cfg_t;

typedef struct {
    uint8_t modules_cfg;        /* сколько источников ожидается */
    uint8_t modules_ok;         /* сколько из них со свежими данными */
    uint8_t units_ok;           /* физических модулей в свежих источниках */
    int64_t newest_us;          /* самое свежее обновление любого источника, 0 — не было */

    uint32_t voltage_mv;        /* среднее по модулям */
    int32_t current_ma;         /* сумма */
    bool soc_valid;
    uint8_t soc_pct;            /* взвешено по ёмкости */
    uint8_t soh_pct;            /* минимум */
    uint16_t cycles_avg;
    uint16_t cycles_max;
    uint32_t capacity_mah;      /* сумма */

    bool cells_valid;
    bool cells_synthetic;       /* по ячейкам известны только min/max */
    uint16_t cell_mv[PACK_SERIES_CELLS]; /* представительные ячейки для Pylontech 0x42 */
    uint16_t cell_min_mv;
    uint16_t cell_max_mv;
    uint8_t cell_min_module;
    uint8_t cell_max_module;

    bool temp_valid;
    int16_t temp_min_dc;
    int16_t temp_max_dc;
    int16_t temp_avg_dc;
    uint8_t temp_min_module;
    uint8_t temp_max_module;

    bool bms_limits_valid;      /* все свежие источники прислали лимиты */
    uint16_t bms_cvl_mv;        /* минимум */
    uint16_t bms_dvl_mv;        /* максимум */
    uint16_t bms_ccl_da;        /* сумма */
    uint16_t bms_dcl_da;
    bool bms_charge_allowed;    /* все свежие источники разрешают */
    bool bms_discharge_allowed;
} pack_summary_t;

/* Чистая функция: сводка по массиву источников на момент now_us. */
void pack_aggregate(const pack_module_t *modules, size_t n, int64_t now_us,
                    const pack_cfg_t *cfg, pack_summary_t *out);

/* Потокобезопасное хранилище (ТЗ 6.2: структура с мьютексом и меткой времени).
 * Нет на хостовом таргете linux. */
void pack_store_init(uint8_t n_modules, const pack_cfg_t *cfg);
void pack_store_put(uint8_t idx, const pack_module_t *m);
void pack_store_get(uint8_t idx, pack_module_t *out);
void pack_store_summary(int64_t now_us, pack_summary_t *out);

#ifdef __cplusplus
}
#endif
