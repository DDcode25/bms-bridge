#pragma once

#include <stddef.h>
#include "limits_calc.h"
#include "pack_model.h"

#define NOW_US 100000000LL   /* 100 с после старта */

extern const limits_cfg_t FX_LIM;
extern const pack_cfg_t FX_PACK;

/* Модуль как у симулятора: 16 × 3300 мВ, 25,0 °C, SOC 60 %, 57,6 А·ч. */
pack_module_t fx_module(void);
/* Сводка по n одинаковым модулям fx_module(). */
pack_summary_t fx_summary(int n);
/* Дописывает к телу кадра "~…INFO" CHKSUM и CR. buf должен вмещать +6. */
size_t fx_frame(char *buf, size_t cap, const char *body);
