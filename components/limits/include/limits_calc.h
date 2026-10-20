/*
 * limits — расчёт лимитов и флагов для инвертора (ТЗ 4.3, 5.2).
 *
 * Главное правило ТЗ: при любой неопределённости шлюз запрещает заряд и
 * разряд или снижает лимиты, а не держит последние значения.
 * Пороги по умолчанию — стартовые для LFP 16S; сверить с порогами BMZ.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "pack_model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t cvl_mv;               /* лимит напряжения заряда, 56,0–56,8 В */
    uint16_t dvl_mv;               /* лимит напряжения разряда, 46,4–48,0 В */
    uint16_t unit_current_da;      /* ток на модуль, 0,1 А: 288 = 28,8 А (0,5C, этикетка) */
    uint16_t unit_taper_current_da;/* 0,1C на модуль: 58 = 5,8 А */

    uint16_t cell_ov_mv;           /* выше — CCL = 0 (3550) */
    uint16_t cell_taper_mv;        /* выше — плавное снижение CCL до 0,1C (3450) */
    uint16_t cell_uv_mv;           /* ниже — DCL = 0 и charge immediately (2900) */
    uint16_t imbalance_mv;         /* разбаланс больше — предупреждение (50) */
    uint16_t imbalance_upper_mv;   /* при разбалансе выше этой ячейки — CCL до 0,1C */

    int16_t charge_temp_min_dc;    /* не выше — CCL = 0 (0 °C) */
    int16_t temp_derate_dc;        /* выше — CCL и DCL вдвое (45 °C) */
    int16_t temp_cutoff_dc;        /* выше — оба 0 (55 °C) */

    uint8_t force_charge_soc;      /* SOC ниже — charge immediately */
} limits_cfg_t;

/* Почему лимиты снижены: битовая маска для лога и веб-страницы. */
enum {
    LIM_NO_DATA        = 1u << 0,
    LIM_NO_CELLS       = 1u << 1,
    LIM_NO_TEMP        = 1u << 2,
    LIM_CELL_OV        = 1u << 3,
    LIM_CELL_TAPER     = 1u << 4,
    LIM_CELL_UV        = 1u << 5,
    LIM_TEMP_LOW       = 1u << 6,
    LIM_TEMP_DERATE    = 1u << 7,
    LIM_TEMP_HIGH      = 1u << 8,
    LIM_IMBALANCE      = 1u << 9,
    LIM_BMS_LIMIT      = 1u << 10,
    LIM_BMS_NO_CHARGE  = 1u << 11,
    LIM_BMS_NO_DISCHG  = 1u << 12,
    LIM_LOW_SOC        = 1u << 13,
};

typedef struct {
    uint16_t cvl_mv;
    uint16_t dvl_mv;
    uint16_t ccl_da;
    uint16_t dcl_da;
    bool charge_enable;
    bool discharge_enable;
    bool charge_immediately;
    uint32_t reasons;

    /* Предупреждения и защиты для Pylontech 0x44 / 0x62. */
    bool warn_cell_high;
    bool warn_cell_low;
    bool warn_temp_high;
    bool warn_temp_low;
    bool warn_imbalance;
    bool prot_cell_ov;
    bool prot_cell_uv;
    bool prot_temp_high;
    bool prot_temp_low;
    bool fault_comm;
} limits_out_t;

void limits_compute(const limits_cfg_t *cfg, const pack_summary_t *pack, limits_out_t *out);

/* Короткая строка с причинами, например "CELL_TAPER,TEMP_DERATE". */
void limits_reasons_str(uint32_t reasons, char *buf, size_t cap);

#ifdef __cplusplus
}
#endif
