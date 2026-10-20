/*
 * Разбор кадров BMZ по гипотезе А (ТЗ 4.1): модуль говорит по CAN 500 кбит/с
 * профилем «CAN-bus BMS» Victron / Pylontech LV CAN V1.3, кадры 0x351…0x379.
 * Масштабы — по спецификации V1.3 и расшифровке в tools/bmz_can_sniffer.
 * Для BMZ это гипотеза: проверить логами этапа 1 с мультиметром и клещами.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "pack_model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int64_t t351, t355, t356, t359, t35c, t373, t379;

    uint16_t cvl_dv;       /* 0x351: 0,1 В */
    int16_t ccl_da;        /* 0x351: 0,1 А */
    int16_t dcl_da;
    uint16_t dvl_dv;
    uint16_t soc_pct;      /* 0x355 */
    uint16_t soh_pct;
    int16_t voltage_cv;    /* 0x356: 0,01 В */
    int16_t current_da;    /* 0x356: 0,1 А */
    int16_t temp_dc;       /* 0x356: 0,1 °C */
    uint8_t protection[2]; /* 0x359 */
    uint8_t alarm[2];
    uint8_t modules;       /* 0x359, байт 4 */
    uint8_t request;       /* 0x35C */
    uint16_t cell_min_mv;  /* 0x373 */
    uint16_t cell_max_mv;
    uint16_t temp_min_k;   /* 0x373: K */
    uint16_t temp_max_k;
    uint16_t capacity_ah;  /* 0x379 */
} bmz_can_state_t;

/* Возвращает true, если ID известен и кадр принят. */
bool bmz_can_feed(bmz_can_state_t *st, uint32_t id, const uint8_t *data, uint8_t dlc, int64_t now_us);

/* Собирает pack_module_t. Свежесть модуля — по 0x356 (напряжение, ток, T). */
void bmz_can_to_module(const bmz_can_state_t *st, int64_t now_us, int64_t stale_us,
                       uint8_t default_units, pack_module_t *out);

#ifdef __cplusplus
}
#endif
