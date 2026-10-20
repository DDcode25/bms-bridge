/*
 * bmz_driver — источник данных о батарее (ТЗ 6.1).
 *
 * Источники (menuconfig → «BMZ: источник данных»):
 *   симулятор — ручные значения, для проверки инвертора без батареи (ТЗ 6.2);
 *   CAN, гипотеза А — кадры 0x351…0x379, см. bmz_can.h.
 * Данные кладутся в pack_store.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bmz_driver_start(void);
uint8_t bmz_driver_module_count(void);
const char *bmz_driver_source_name(void);

/* Симулятор: значения задаются из консоли (команды sim …). */
void bmz_sim_set_cells(uint16_t mv);
void bmz_sim_set_cell(uint8_t idx, uint16_t mv);
void bmz_sim_set_temp(int16_t dc);
void bmz_sim_set_current(int32_t ma);
void bmz_sim_set_soc(uint8_t pct);
void bmz_sim_set_paused(bool paused);        /* T4: «кабель X1 отключён» */
void bmz_sim_set_module_online(uint8_t idx, bool online);  /* T9 */

#ifdef __cplusplus
}
#endif
