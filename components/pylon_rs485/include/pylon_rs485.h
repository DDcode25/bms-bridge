/*
 * pylon_rs485 — ответчик инвертору по Pylontech RS485 (UART, half-duplex).
 * Инвертор — ведущий, шлюз — ведомый (ТЗ 4.2).
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "limits_calc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t frames;          /* кадров с правильным SOI/EOI */
    uint32_t for_us;          /* из них на наш адрес */
    uint32_t responses;
    uint32_t errors;          /* ответили кодом ошибки */
    uint32_t garbage;         /* мусор / переполнение буфера */
    uint32_t silenced;        /* запросы без ответа: нет данных BMZ дольше таймаута */
    int64_t last_request_us;  /* 0 — запросов не было */
} pylon_rs485_stats_t;

esp_err_t pylon_rs485_start(const limits_cfg_t *lim_cfg);
void pylon_rs485_get_stats(pylon_rs485_stats_t *out);
/* Сколько раз инвертор запрашивал каждый CID2 (этап 2 ТЗ: список команд SMH III). */
void pylon_rs485_print_cid2(void);

#ifdef __cplusplus
}
#endif
