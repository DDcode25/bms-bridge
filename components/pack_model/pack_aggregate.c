#include <limits.h>
#include <string.h>
#include "pack_model.h"

static bool is_fresh(const pack_module_t *m, int64_t now_us, int64_t stale_us)
{
    return m->updated_us != 0 && now_us - m->updated_us <= stale_us;
}

static void module_cell_range(const pack_module_t *m, uint16_t *lo, uint16_t *hi)
{
    if (m->n_cells == 0) {
        *lo = m->cell_min_mv;
        *hi = m->cell_max_mv;
        return;
    }
    *lo = UINT16_MAX;
    *hi = 0;
    for (int i = 0; i < m->n_cells && i < PACK_SERIES_CELLS; i++) {
        if (m->cell_mv[i] < *lo) {
            *lo = m->cell_mv[i];
        }
        if (m->cell_mv[i] > *hi) {
            *hi = m->cell_mv[i];
        }
    }
}

void pack_aggregate(const pack_module_t *modules, size_t n, int64_t now_us,
                    const pack_cfg_t *cfg, pack_summary_t *s)
{
    memset(s, 0, sizeof(*s));
    s->modules_cfg = (uint8_t)n;

    uint64_t v_sum = 0;
    int64_t i_sum = 0;
    uint64_t cap_sum = 0;
    uint64_t soc_weighted = 0;   /* мА·ч × % */
    uint64_t soc_cap = 0;
    uint32_t cycles_sum = 0;
    int32_t t_sum = 0;
    int t_count = 0;
    bool any_cells_full = false;
    bool cells_known = true;

    s->soh_pct = 100;
    s->cell_min_mv = UINT16_MAX;
    s->temp_min_dc = INT16_MAX;
    s->temp_max_dc = INT16_MIN;
    s->bms_limits_valid = true;
    s->bms_cvl_mv = UINT16_MAX;
    s->bms_charge_allowed = true;
    s->bms_discharge_allowed = true;

    uint32_t ccl_sum = 0, dcl_sum = 0;

    for (size_t idx = 0; idx < n; idx++) {
        const pack_module_t *m = &modules[idx];
        if (m->updated_us > s->newest_us) {
            s->newest_us = m->updated_us;
        }
        if (!is_fresh(m, now_us, cfg->stale_us)) {
            continue; /* ТЗ 5.2: выпавший модуль исключается из расчёта */
        }
        uint8_t units = m->units ? m->units : 1;
        s->modules_ok++;
        s->units_ok += units;

        v_sum += m->voltage_mv;
        i_sum += m->current_ma;

        uint32_t cap = m->capacity_mah ? m->capacity_mah : cfg->unit_capacity_mah * units;
        cap_sum += cap;
        if (m->soc_pct <= 100) {
            soc_weighted += (uint64_t)cap * m->soc_pct;
            soc_cap += cap;
        }
        if (m->soh_pct && m->soh_pct < s->soh_pct) {
            s->soh_pct = m->soh_pct;
        }
        cycles_sum += m->cycles;
        if (m->cycles > s->cycles_max) {
            s->cycles_max = m->cycles;
        }

        uint16_t lo, hi;
        module_cell_range(m, &lo, &hi);
        if (lo == 0 || hi == 0) {
            cells_known = false;
        } else {
            if (lo < s->cell_min_mv) {
                s->cell_min_mv = lo;
                s->cell_min_module = (uint8_t)idx;
            }
            if (hi > s->cell_max_mv) {
                s->cell_max_mv = hi;
                s->cell_max_module = (uint8_t)idx;
            }
        }
        if (m->n_cells == PACK_SERIES_CELLS) {
            any_cells_full = true;
        }

        for (int t = 0; t < m->n_temps && t < PACK_MAX_TEMPS; t++) {
            int16_t v = m->temp_dc[t];
            t_sum += v;
            t_count++;
            if (v < s->temp_min_dc) {
                s->temp_min_dc = v;
                s->temp_min_module = (uint8_t)idx;
            }
            if (v > s->temp_max_dc) {
                s->temp_max_dc = v;
                s->temp_max_module = (uint8_t)idx;
            }
        }

        if (m->bms_limits_valid) {
            if (m->bms_cvl_mv && m->bms_cvl_mv < s->bms_cvl_mv) {
                s->bms_cvl_mv = m->bms_cvl_mv;
            }
            if (m->bms_dvl_mv > s->bms_dvl_mv) {
                s->bms_dvl_mv = m->bms_dvl_mv;
            }
            ccl_sum += m->bms_ccl_da;
            dcl_sum += m->bms_dcl_da;
        } else {
            s->bms_limits_valid = false;
        }
        s->bms_charge_allowed &= m->bms_charge_allowed;
        s->bms_discharge_allowed &= m->bms_discharge_allowed;
    }

    if (s->modules_ok == 0) {
        memset(s, 0, sizeof(*s));
        s->modules_cfg = (uint8_t)n;
        for (size_t idx = 0; idx < n; idx++) {
            if (modules[idx].updated_us > s->newest_us) {
                s->newest_us = modules[idx].updated_us;
            }
        }
        return;
    }

    s->voltage_mv = (uint32_t)(v_sum / s->modules_ok);
    s->current_ma = (int32_t)i_sum;
    s->capacity_mah = (uint32_t)cap_sum;
    s->cycles_avg = (uint16_t)(cycles_sum / s->modules_ok);
    if (soc_cap) {
        s->soc_valid = true;
        s->soc_pct = (uint8_t)((soc_weighted + soc_cap / 2) / soc_cap);
    }

    s->cells_valid = cells_known;
    if (!cells_known) {
        s->cell_min_mv = 0;
        s->cell_max_mv = 0;
    } else if (any_cells_full) {
        /* Модули в параллель: по каждой позиции берём худшую ячейку —
         * максимальную при заряде, минимальную при разряде. */
        bool charging = s->current_ma >= 0;
        for (int c = 0; c < PACK_SERIES_CELLS; c++) {
            uint16_t v = charging ? 0 : UINT16_MAX;
            for (size_t idx = 0; idx < n; idx++) {
                const pack_module_t *m = &modules[idx];
                if (!is_fresh(m, now_us, cfg->stale_us) || m->n_cells != PACK_SERIES_CELLS) {
                    continue;
                }
                if (charging ? m->cell_mv[c] > v : m->cell_mv[c] < v) {
                    v = m->cell_mv[c];
                }
            }
            s->cell_mv[c] = v;
        }
    } else {
        /* Известны только min/max: среднее по ячейке, а крайние значения —
         * в первых двух позициях, чтобы инвертор их видел. */
        s->cells_synthetic = true;
        uint32_t avg = s->voltage_mv / PACK_SERIES_CELLS;
        if (avg < s->cell_min_mv) {
            avg = s->cell_min_mv;
        }
        if (avg > s->cell_max_mv) {
            avg = s->cell_max_mv;
        }
        for (int c = 0; c < PACK_SERIES_CELLS; c++) {
            s->cell_mv[c] = (uint16_t)avg;
        }
        s->cell_mv[0] = s->cell_max_mv;
        s->cell_mv[1] = s->cell_min_mv;
    }

    if (t_count) {
        s->temp_valid = true;
        s->temp_avg_dc = (int16_t)(t_sum / t_count);
    } else {
        s->temp_min_dc = 0;
        s->temp_max_dc = 0;
    }

    if (s->bms_limits_valid) {
        s->bms_ccl_da = ccl_sum > UINT16_MAX ? UINT16_MAX : (uint16_t)ccl_sum;
        s->bms_dcl_da = dcl_sum > UINT16_MAX ? UINT16_MAX : (uint16_t)dcl_sum;
        if (s->bms_cvl_mv == UINT16_MAX) {
            s->bms_cvl_mv = 0;
        }
    } else {
        s->bms_cvl_mv = 0;
        s->bms_dvl_mv = 0;
    }
}
