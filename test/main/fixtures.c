#include <stdio.h>
#include <string.h>
#include "fixtures.h"
#include "pylon_frame.h"

const limits_cfg_t FX_LIM = {
    .cvl_mv = 56000,
    .dvl_mv = 48000,
    .unit_current_da = 288,
    .unit_taper_current_da = 58,
    .cell_ov_mv = 3550,
    .cell_taper_mv = 3450,
    .cell_uv_mv = 2900,
    .imbalance_mv = 50,
    .imbalance_upper_mv = 3400,
    .charge_temp_min_dc = 0,
    .temp_derate_dc = 450,
    .temp_cutoff_dc = 550,
    .force_charge_soc = 5,
};

const pack_cfg_t FX_PACK = {
    .stale_us = 10000000,
    .unit_capacity_mah = 57600,
};

pack_module_t fx_module(void)
{
    pack_module_t m = {0};
    m.updated_us = NOW_US - 500000;
    m.units = 1;
    m.n_cells = PACK_SERIES_CELLS;
    for (int i = 0; i < PACK_SERIES_CELLS; i++) {
        m.cell_mv[i] = 3300;
        m.voltage_mv += 3300;
    }
    m.current_ma = 10000;
    m.soc_pct = 60;
    m.soh_pct = 100;
    m.cycles = 10;
    m.capacity_mah = 57600;
    m.n_temps = 4;
    for (int i = 0; i < 4; i++) {
        m.temp_dc[i] = 250;
    }
    m.bms_charge_allowed = true;
    m.bms_discharge_allowed = true;
    return m;
}

pack_summary_t fx_summary(int n)
{
    pack_module_t m[PACK_MAX_MODULES];
    for (int i = 0; i < n; i++) {
        m[i] = fx_module();
    }
    pack_summary_t s;
    pack_aggregate(m, (size_t)n, NOW_US, &FX_PACK, &s);
    return s;
}

size_t fx_frame(char *buf, size_t cap, const char *body)
{
    size_t n = strlen(body);
    if (n + 6 > cap) {
        return 0;
    }
    memcpy(buf, body, n);
    snprintf(buf + n, cap - n, "%04X\r", pylon_checksum(body + 1, n - 1));
    return n + 5;
}
