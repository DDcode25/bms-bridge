#include <string.h>
#include "bmz_can.h"

static uint16_t u16le(const uint8_t *d) { return (uint16_t)(d[0] | (d[1] << 8)); }
static int16_t s16le(const uint8_t *d) { return (int16_t)(d[0] | (d[1] << 8)); }

bool bmz_can_feed(bmz_can_state_t *st, uint32_t id, const uint8_t *d, uint8_t n, int64_t now)
{
    switch (id) {
    case 0x351:   /* CVL, CCL, DCL, DVL */
        if (n < 6) {
            return false;
        }
        st->cvl_dv = u16le(d);
        st->ccl_da = s16le(d + 2);
        st->dcl_da = s16le(d + 4);
        st->dvl_dv = n >= 8 ? u16le(d + 6) : 0;
        st->t351 = now;
        return true;
    case 0x355:   /* SOC, SOH, % */
        if (n < 4) {
            return false;
        }
        st->soc_pct = u16le(d);
        st->soh_pct = u16le(d + 2);
        st->t355 = now;
        return true;
    case 0x356:   /* U 0,01 В, I 0,1 А, T 0,1 °C */
        if (n < 6) {
            return false;
        }
        st->voltage_cv = s16le(d);
        st->current_da = s16le(d + 2);
        st->temp_dc = s16le(d + 4);
        st->t356 = now;
        return true;
    case 0x359:   /* защиты, аварии, число модулей */
        if (n < 4) {
            return false;
        }
        st->protection[0] = d[0];
        st->protection[1] = d[1];
        st->alarm[0] = d[2];
        st->alarm[1] = d[3];
        st->modules = n >= 5 ? d[4] : 0;
        st->t359 = now;
        return true;
    case 0x35C:   /* запросы: бит 7 — заряд разрешён, бит 6 — разряд разрешён */
        if (n < 1) {
            return false;
        }
        st->request = d[0];
        st->t35c = now;
        return true;
    case 0x373:   /* min/max ячейки, мВ; min/max температуры, K */
        if (n < 8) {
            return false;
        }
        st->cell_min_mv = u16le(d);
        st->cell_max_mv = u16le(d + 2);
        st->temp_min_k = u16le(d + 4);
        st->temp_max_k = u16le(d + 6);
        st->t373 = now;
        return true;
    case 0x379:   /* установленная ёмкость, А·ч */
        if (n < 2) {
            return false;
        }
        st->capacity_ah = u16le(d);
        st->t379 = now;
        return true;
    default:
        return false;
    }
}

static bool fresh(int64_t t, int64_t now, int64_t stale) { return t != 0 && now - t <= stale; }

static int16_t kelvin_to_dc(uint16_t k) { return (int16_t)(k * 10 - 2731); }

void bmz_can_to_module(const bmz_can_state_t *st, int64_t now, int64_t stale,
                       uint8_t default_units, pack_module_t *m)
{
    memset(m, 0, sizeof(*m));
    m->updated_us = st->t356;
    m->units = (fresh(st->t359, now, stale) && st->modules) ? st->modules : default_units;

    m->voltage_mv = st->voltage_cv > 0 ? (uint32_t)st->voltage_cv * 10 : 0;
    m->current_ma = (int32_t)st->current_da * 100;
    m->soc_pct = fresh(st->t355, now, stale) && st->soc_pct <= 100 ? (uint8_t)st->soc_pct
                                                                  : PACK_SOC_UNKNOWN;
    m->soh_pct = fresh(st->t355, now, stale) && st->soh_pct <= 100 ? (uint8_t)st->soh_pct : 0;
    m->capacity_mah = fresh(st->t379, now, stale) ? (uint32_t)st->capacity_ah * 1000 : 0;

    /* По ячейкам профиль даёт только min/max (0x373). Без 0x373 ячейки
     * неизвестны, и limits запретит заряд и разряд. */
    if (fresh(st->t373, now, stale)) {
        m->cell_min_mv = st->cell_min_mv;
        m->cell_max_mv = st->cell_max_mv;
    }
    m->temp_dc[m->n_temps++] = st->temp_dc;
    if (fresh(st->t373, now, stale)) {
        m->temp_dc[m->n_temps++] = kelvin_to_dc(st->temp_min_k);
        m->temp_dc[m->n_temps++] = kelvin_to_dc(st->temp_max_k);
    }

    if (fresh(st->t351, now, stale)) {
        m->bms_limits_valid = true;
        m->bms_cvl_mv = (uint16_t)(st->cvl_dv * 100);
        m->bms_dvl_mv = (uint16_t)(st->dvl_dv * 100);
        m->bms_ccl_da = st->ccl_da > 0 ? (uint16_t)st->ccl_da : 0;
        m->bms_dcl_da = st->dcl_da > 0 ? (uint16_t)st->dcl_da : 0;
    }

    /* Если BMS не шлёт 0x35C, своих запретов у неё нет — решает limits.
     * Биты защит 0x359 — по Pylontech LV CAN V1.3. */
    bool charge = true, discharge = true;
    if (fresh(st->t35c, now, stale)) {
        charge = st->request & 0x80;
        discharge = st->request & 0x40;
    }
    if (fresh(st->t359, now, stale)) {
        uint8_t p0 = st->protection[0], p1 = st->protection[1];
        if (p0 & 0x02) { charge = false; }                    /* перенапряжение */
        if (p0 & 0x04) { discharge = false; }                 /* недонапряжение */
        if (p0 & 0x08) { charge = false; discharge = false; } /* перегрев */
        if (p0 & 0x10) { charge = false; }                    /* переохлаждение */
        if (p0 & 0x80) { discharge = false; }                 /* перегрузка по разряду */
        if (p1 & 0x01) { charge = false; }                    /* перегрузка по заряду */
        if (p1 & 0x08) { charge = false; discharge = false; } /* внутренняя ошибка BMS */
    }
    m->bms_charge_allowed = charge;
    m->bms_discharge_allowed = discharge;
}
