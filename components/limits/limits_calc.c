#include <stdio.h>
#include <string.h>
#include "limits_calc.h"

static uint32_t min_u32(uint32_t a, uint32_t b) { return a < b ? a : b; }

void limits_compute(const limits_cfg_t *c, const pack_summary_t *p, limits_out_t *o)
{
    memset(o, 0, sizeof(*o));
    o->cvl_mv = c->cvl_mv;
    o->dvl_mv = c->dvl_mv;

    /* ТЗ 5.2: нет данных от BMZ / перезагрузка до первых данных — CCL = DCL = 0. */
    if (p->modules_ok == 0) {
        o->reasons = LIM_NO_DATA;
        o->fault_comm = true;
        return;
    }
    /* Без ячеек или температур защиту не проверить — это неопределённость. */
    if (!p->cells_valid) {
        o->reasons |= LIM_NO_CELLS;
    }
    if (!p->temp_valid) {
        o->reasons |= LIM_NO_TEMP;
    }
    if (o->reasons) {
        return;
    }

    uint32_t base = (uint32_t)c->unit_current_da * p->units_ok;
    uint32_t taper = (uint32_t)c->unit_taper_current_da * p->units_ok;
    if (taper > base) {
        taper = base;
    }
    uint32_t ccl = base;
    uint32_t dcl = base;
    bool force = false;

    /* Ячейки */
    if (p->cell_max_mv > c->cell_ov_mv) {
        ccl = 0;
        o->reasons |= LIM_CELL_OV;
        o->prot_cell_ov = true;
    } else if (p->cell_max_mv > c->cell_taper_mv && c->cell_ov_mv > c->cell_taper_mv) {
        /* Линейно от base на cell_taper_mv до 0,1C на cell_ov_mv. */
        uint32_t span = c->cell_ov_mv - c->cell_taper_mv;
        uint32_t left = c->cell_ov_mv - p->cell_max_mv;
        ccl = taper + (base - taper) * left / span;
        o->reasons |= LIM_CELL_TAPER;
        o->warn_cell_high = true;
    }
    if (p->cell_min_mv < c->cell_uv_mv) {
        dcl = 0;
        force = true;
        o->reasons |= LIM_CELL_UV;
        o->prot_cell_uv = true;
        o->warn_cell_low = true;
    }
    uint16_t spread = p->cell_max_mv - p->cell_min_mv;
    if (spread > c->imbalance_mv) {
        o->reasons |= LIM_IMBALANCE;
        o->warn_imbalance = true;
        if (p->cell_max_mv >= c->imbalance_upper_mv) {
            ccl = min_u32(ccl, taper);
        }
    }

    /* Температура */
    /* ТЗ 5.2 говорит «ниже 0 °C», а T7 ждёт CCL = 0 при 0 °C — берём строже: ≤. */
    if (p->temp_min_dc <= c->charge_temp_min_dc) {
        ccl = 0;
        o->reasons |= LIM_TEMP_LOW;
        o->warn_temp_low = true;
        o->prot_temp_low = true;
    }
    if (p->temp_max_dc > c->temp_cutoff_dc) {
        ccl = 0;
        dcl = 0;
        o->reasons |= LIM_TEMP_HIGH;
        o->prot_temp_high = true;
        o->warn_temp_high = true;
    } else if (p->temp_max_dc > c->temp_derate_dc) {
        ccl /= 2;
        dcl /= 2;
        o->reasons |= LIM_TEMP_DERATE;
        o->warn_temp_high = true;
    }

    /* Лимиты и запреты самой BMS: берём более строгое. */
    if (p->bms_limits_valid) {
        if (p->bms_ccl_da < ccl || p->bms_dcl_da < dcl) {
            o->reasons |= LIM_BMS_LIMIT;
        }
        ccl = min_u32(ccl, p->bms_ccl_da);
        dcl = min_u32(dcl, p->bms_dcl_da);
        if (p->bms_cvl_mv && p->bms_cvl_mv < o->cvl_mv) {
            o->cvl_mv = p->bms_cvl_mv;
        }
        if (p->bms_dvl_mv > o->dvl_mv) {
            o->dvl_mv = p->bms_dvl_mv;
        }
    }
    if (!p->bms_charge_allowed) {
        ccl = 0;
        o->reasons |= LIM_BMS_NO_CHARGE;
    }
    if (!p->bms_discharge_allowed) {
        dcl = 0;
        o->reasons |= LIM_BMS_NO_DISCHG;
    }

    if (p->soc_valid && p->soc_pct < c->force_charge_soc) {
        force = true;
        o->reasons |= LIM_LOW_SOC;
    }

    o->ccl_da = (uint16_t)min_u32(ccl, UINT16_MAX);
    o->dcl_da = (uint16_t)min_u32(dcl, UINT16_MAX);
    o->charge_enable = o->ccl_da > 0;
    o->discharge_enable = o->dcl_da > 0;
    o->charge_immediately = force && o->charge_enable;
}

void limits_reasons_str(uint32_t r, char *buf, size_t cap)
{
    static const char *const names[] = {
        "NO_DATA", "NO_CELLS", "NO_TEMP", "CELL_OV", "CELL_TAPER", "CELL_UV",
        "TEMP_LOW", "TEMP_DERATE", "TEMP_HIGH", "IMBALANCE", "BMS_LIMIT",
        "BMS_NO_CHARGE", "BMS_NO_DISCHG", "LOW_SOC",
    };
    size_t len = 0;
    if (cap == 0) {
        return;
    }
    buf[0] = '\0';
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        if (r & (1u << i)) {
            int n = snprintf(buf + len, cap - len, "%s%s", len ? "," : "", names[i]);
            if (n < 0 || (size_t)n >= cap - len) {
                break;
            }
            len += (size_t)n;
        }
    }
    if (len == 0) {
        snprintf(buf, cap, "-");
    }
}
