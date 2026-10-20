/*
 * Ответы шлюза на запросы инвертора по Pylontech RS485 (ТЗ 4.2).
 * Чистые функции без UART, покрыты тестами в test/.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "limits_calc.h"
#include "pack_model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t adr;              /* адрес батареи, первая батарея = 0x02 (V3.5, стр. 3) */
    uint8_t sys_adr;          /* адрес мастера для «системных» 0x60–0x64: 0x12 (V3.5, 2) */
    uint8_t proto_ver;        /* VER в ответе на 0x4F */
    uint16_t current_unit_ma; /* шаг тока в 0x42: 100 мА (V2.5) или 10 мА */
    bool dcl_negative;        /* DCL в 0x92 со знаком «−» */
    const char *device_name;  /* 0x51/0x60: 10 байт */
    const char *manufacturer; /* 0x51/0x60: 20 байт */
    uint16_t sw_version;
    const char *serial;       /* 0x93/0x60: 16 байт */
} pylon_cfg_t;

typedef struct {
    const pack_summary_t *pack;
    const limits_out_t *lim;
    const limits_cfg_t *lim_cfg;
} pylon_data_t;

typedef struct {
    int parse_rc;     /* PYLON_RTN_* или PYLON_PARSE_NOT_FRAME */
    uint8_t ver;
    uint8_t adr;
    uint8_t cid2;
    uint8_t rtn;      /* что ответили */
    bool for_us;
} pylon_req_info_t;

/* Разбирает запрос и пишет ответ в out. Возвращает длину ответа
 * или 0, если отвечать не нужно (не кадр, чужой адрес). */
size_t pylon_handle_request(const pylon_cfg_t *cfg, const pylon_data_t *d,
                            const char *req, size_t req_len,
                            char *out, size_t cap, pylon_req_info_t *info);

#ifdef __cplusplus
}
#endif
