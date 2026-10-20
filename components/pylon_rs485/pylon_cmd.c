/*
 * Команды Pylontech RS485.
 *
 * Источники форматов (PDF в inverters/protocols/):
 *   0x42, 0x44, 0x47, 0x4F, 0x51, 0x92, 0x93, 0x96 — PYLON RS485 LV V3.3
 *               (20180821), раздел 3;
 *   0x60–0x63 — V3.5 (20190807), раздел 2.
 * Всю батарею шлюз отдаёт как ОДИН модуль Pylontech с суммарными данными (ТЗ 4.3).
 */
#include <string.h>
#include "pylon_cmd.h"
#include "pylon_frame.h"

#define CELLS        PACK_SERIES_CELLS
#define TEMPS        5       /* как у Pylontech US: BMS + 4 группы ячеек */
#define KELVIN_DK    2731    /* 0,1 K = 0,1 °C + 2731 (V3.5, 2.2, п. 12) */

/* Нижняя температура разряда для 0x47: для LFP обычно −20 °C. */
#define DISCHARGE_TEMP_MIN_DC (-200)
/* Порог «under voltage» ячейки для 0x47: на 100 мВ ниже порога DCL = 0. */
#define CELL_UNDER_MARGIN_MV  100

#define INFOFLAG_ALARM 0x10  /* V3.5, 1.2.2: есть непрочитанные изменения аварий */
#define CELL_ERROR_HIGH_MV 4200  /* V3.3, 3.5, Status 4/5: ячейка «неисправна» */
#define CELL_ERROR_LOW_MV  1000
#define EFFECTIVE_CURRENT_MA 100 /* V3.3, 3.5, Status 3: «эффективный» ток > 0,1 А */

static int16_t clamp_i16(int32_t v)
{
    return v > INT16_MAX ? INT16_MAX : v < INT16_MIN ? INT16_MIN : (int16_t)v;
}

static uint16_t kelvin(int16_t dc) { return (uint16_t)clamp_i16(dc + KELVIN_DK); }

static bool any_alarm(const limits_out_t *l)
{
    return l->warn_cell_high || l->warn_cell_low || l->warn_temp_high || l->warn_temp_low ||
           l->warn_imbalance || l->prot_cell_ov || l->prot_cell_uv || l->prot_temp_high ||
           l->prot_temp_low || l->fault_comm;
}

/* Температуры для 0x42/0x44: [BMS, группы 1–4]. Отдельных датчиков BMZ пока
 * не знаем, поэтому в группах — min и max, чтобы инвертор видел крайние. */
static void temps5(const pack_summary_t *p, int16_t t[TEMPS])
{
    t[0] = p->temp_avg_dc;
    t[1] = p->temp_min_dc;
    t[2] = p->temp_max_dc;
    t[3] = p->temp_avg_dc;
    t[4] = p->temp_avg_dc;
}

static uint8_t soc_or_zero(const pack_summary_t *p) { return p->soc_valid ? p->soc_pct : 0; }

static void put_analog_module(pylon_writer_t *w, const pylon_cfg_t *cfg, const pack_summary_t *p)
{
    pylon_put_u8(w, CELLS);
    for (int i = 0; i < CELLS; i++) {
        pylon_put_u16(w, p->cell_mv[i]);
    }
    int16_t t[TEMPS];
    temps5(p, t);
    pylon_put_u8(w, TEMPS);
    for (int i = 0; i < TEMPS; i++) {
        pylon_put_u16(w, kelvin(t[i]));
    }
    int32_t unit = cfg->current_unit_ma ? cfg->current_unit_ma : 100;
    pylon_put_u16(w, (uint16_t)clamp_i16(p->current_ma / unit));
    pylon_put_u16(w, (uint16_t)(p->voltage_mv > UINT16_MAX ? UINT16_MAX : p->voltage_mv));

    uint32_t total = p->capacity_mah;
    uint32_t remain = (uint32_t)((uint64_t)total * soc_or_zero(p) / 100);
    if (total > UINT16_MAX) {
        /* V3.2: ёмкость больше 65 А·ч — USER DEFINED = 4 и 24-битные поля. */
        pylon_put_u16(w, 0xFFFF);
        pylon_put_u8(w, 4);
        pylon_put_u16(w, 0xFFFF);
        pylon_put_u16(w, p->cycles_max);
        pylon_put_u24(w, remain);
        pylon_put_u24(w, total);
    } else {
        pylon_put_u16(w, (uint16_t)remain);
        pylon_put_u8(w, 2);
        pylon_put_u16(w, (uint16_t)total);
        pylon_put_u16(w, p->cycles_max);
    }
}

static void cmd_42(pylon_writer_t *w, const pylon_cfg_t *cfg, const pylon_data_t *d, uint8_t command)
{
    pylon_put_u8(w, any_alarm(d->lim) ? INFOFLAG_ALARM : 0);
    pylon_put_u8(w, command == 0xFF ? 1 : command);  /* 0xFF — все модули: число модулей */
    put_analog_module(w, cfg, d->pack);
}

/* Статус значения в 0x44 (V3.3, 3.5): 0 — норма, 1 — ниже нижнего порога,
 * 2 — выше верхнего (оба — уже защита). */
static uint8_t level(bool low, bool high) { return high ? 2 : low ? 1 : 0; }

static void cmd_44(pylon_writer_t *w, const pylon_data_t *d, uint8_t command)
{
    const pack_summary_t *p = d->pack;
    const limits_cfg_t *c = d->lim_cfg;
    const limits_out_t *l = d->lim;
    uint16_t under_mv = c->cell_uv_mv - CELL_UNDER_MARGIN_MV;
    bool module_ov = p->voltage_mv > (uint32_t)c->cell_ov_mv * CELLS;
    bool module_uv = p->voltage_mv && p->voltage_mv < (uint32_t)under_mv * CELLS;

    pylon_put_u8(w, any_alarm(l) ? INFOFLAG_ALARM : 0);
    pylon_put_u8(w, command == 0xFF ? 1 : command);
    pylon_put_u8(w, CELLS);
    uint16_t cell_errors = 0;
    for (int i = 0; i < CELLS; i++) {
        uint16_t v = p->cell_mv[i];
        pylon_put_u8(w, level(v && v < c->cell_uv_mv, v > c->cell_ov_mv));
        if (v > CELL_ERROR_HIGH_MV || (v && v < CELL_ERROR_LOW_MV)) {
            cell_errors |= (uint16_t)(1u << i);
        }
    }
    int16_t t[TEMPS];
    temps5(p, t);
    pylon_put_u8(w, TEMPS);
    for (int i = 0; i < TEMPS; i++) {
        pylon_put_u8(w, level(t[i] <= c->charge_temp_min_dc, t[i] > c->temp_cutoff_dc));
    }
    pylon_put_u8(w, 0);                              /* ток заряда: перегрузку не считаем */
    pylon_put_u8(w, level(module_uv, module_ov));    /* напряжение модуля */
    pylon_put_u8(w, 0);                              /* ток разряда */

    uint8_t st1 = 0;
    st1 |= module_uv ? 0x80 : 0;                     /* module under voltage */
    st1 |= l->prot_temp_high ? 0x40 | 0x20 : 0;      /* charge / discharge over temperature */
    st1 |= l->prot_cell_uv ? 0x02 : 0;               /* cell under voltage */
    st1 |= module_ov ? 0x01 : 0;                     /* module over voltage */
    uint8_t st2 = 0x08;                              /* питание от модуля */
    st2 |= l->discharge_enable ? 0x04 : 0;           /* discharge MOSFET */
    st2 |= l->charge_enable ? 0x02 : 0;              /* charge MOSFET */
    uint8_t st3 = 0;
    st3 |= p->current_ma > EFFECTIVE_CURRENT_MA ? 0x80 : 0;
    st3 |= p->current_ma < -EFFECTIVE_CURRENT_MA ? 0x40 : 0;
    st3 |= p->soc_valid && p->soc_pct >= 100 ? 0x08 : 0;
    pylon_put_u8(w, st1);
    pylon_put_u8(w, st2);
    pylon_put_u8(w, st3);
    pylon_put_u8(w, (uint8_t)(cell_errors & 0xFF));  /* Status 4: ячейки 1–8 */
    pylon_put_u8(w, (uint8_t)(cell_errors >> 8));    /* Status 5: ячейки 9–16 */
}

static void cmd_47(pylon_writer_t *w, const pylon_data_t *d)
{
    const limits_cfg_t *c = d->lim_cfg;
    uint16_t under = c->cell_uv_mv - CELL_UNDER_MARGIN_MV;
    pylon_put_u8(w, any_alarm(d->lim) ? INFOFLAG_ALARM : 0);
    pylon_put_u16(w, c->cell_ov_mv);
    pylon_put_u16(w, c->cell_uv_mv);
    pylon_put_u16(w, under);
    pylon_put_u16(w, kelvin(c->temp_cutoff_dc));
    pylon_put_u16(w, kelvin(c->charge_temp_min_dc));
    pylon_put_u16(w, d->lim->ccl_da);
    pylon_put_u16(w, (uint16_t)(c->cell_ov_mv * CELLS));
    pylon_put_u16(w, d->lim->dvl_mv);
    pylon_put_u16(w, (uint16_t)(under * CELLS));
    pylon_put_u16(w, kelvin(c->temp_cutoff_dc));
    pylon_put_u16(w, kelvin(DISCHARGE_TEMP_MIN_DC));
    pylon_put_u16(w, d->lim->dcl_da);
}

static void cmd_51(pylon_writer_t *w, const pylon_cfg_t *cfg)
{
    pylon_put_text(w, cfg->device_name, 10);
    pylon_put_u16(w, cfg->sw_version);
    pylon_put_text(w, cfg->manufacturer, 20);
}

/* Бит 7 — заряд разрешён, бит 6 — разряд разрешён. «Charge immediately»:
 * в 0x92 (V3.3, 3.6) два бита — 5 и 4, V3.3 советует инвертору понимать оба,
 * поэтому ставим оба; в 0x63 (V3.5, 2.4) — только бит 5, а бит 4 там
 * «full charge request». */
static uint8_t mgmt_status(const limits_out_t *l, uint8_t immediately_bits)
{
    uint8_t s = 0;
    if (l->charge_enable) {
        s |= 0x80;
    }
    if (l->discharge_enable) {
        s |= 0x40;
    }
    if (l->charge_immediately) {
        s |= immediately_bits;
    }
    return s;
}

static void cmd_92(pylon_writer_t *w, const pylon_cfg_t *cfg, const pylon_data_t *d, uint8_t command)
{
    const limits_out_t *l = d->lim;
    pylon_put_u8(w, command);
    pylon_put_u16(w, l->cvl_mv);
    pylon_put_u16(w, l->dvl_mv);
    pylon_put_u16(w, l->ccl_da);
    pylon_put_u16(w, (uint16_t)(cfg->dcl_negative ? -(int32_t)l->dcl_da : l->dcl_da));
    pylon_put_u8(w, mgmt_status(l, 0x30));
}

static void cmd_93(pylon_writer_t *w, const pylon_cfg_t *cfg, uint8_t command)
{
    pylon_put_u8(w, command);
    pylon_put_text(w, cfg->serial, 16);
}

/* 0x96: 2 байта версии производителя + 3 байта «main line» (V3.3, 3.10). */
static void cmd_96(pylon_writer_t *w, const pylon_cfg_t *cfg, uint8_t command)
{
    pylon_put_u8(w, command);
    pylon_put_u16(w, cfg->sw_version);
    pylon_put_u24(w, cfg->sw_version);
}

static void cmd_60(pylon_writer_t *w, const pylon_cfg_t *cfg)
{
    pylon_put_text(w, cfg->device_name, 10);
    pylon_put_text(w, cfg->manufacturer, 20);
    pylon_put_u16(w, cfg->sw_version);
    pylon_put_u8(w, 1);
    pylon_put_text(w, cfg->serial, 16);
}

/* «Модуль» в 0x61: 0x00GA — G группа, A адрес модуля. В примере V3.5, 2.2
 * сырые байты дают 0x0034 («34H: группа 3, адрес 4»), хотя таблица
 * расшифровки там же пишет 0x0304 — верим кадру, его CHKSUM сходится. */
static uint16_t location(const pylon_cfg_t *cfg, uint8_t module)
{
    return (uint16_t)(((cfg->sys_adr & 0xF0) | ((cfg->adr + module) & 0x0F)));
}

static void cmd_61(pylon_writer_t *w, const pylon_cfg_t *cfg, const pylon_data_t *d)
{
    const pack_summary_t *p = d->pack;
    pylon_put_u16(w, (uint16_t)(p->voltage_mv > UINT16_MAX ? UINT16_MAX : p->voltage_mv));
    pylon_put_u16(w, (uint16_t)clamp_i16(p->current_ma / 10));   /* 10 мА: 0x61A8 = 25,00 А */
    pylon_put_u8(w, soc_or_zero(p));
    pylon_put_u16(w, p->cycles_avg);
    pylon_put_u16(w, p->cycles_max);
    pylon_put_u8(w, p->soh_pct);
    pylon_put_u8(w, p->soh_pct);
    pylon_put_u16(w, p->cell_max_mv);
    pylon_put_u16(w, location(cfg, p->cell_max_module));
    pylon_put_u16(w, p->cell_min_mv);
    pylon_put_u16(w, location(cfg, p->cell_min_module));
    /* Ячейки, MOSFET, BMS. Отдельных датчиков MOSFET и BMS у BMZ пока не знаем —
     * повторяем температуры ячеек, как Fahmula; 0xFF-заглушка из V3.5 часть
     * инверторов прочтёт как реальную температуру. */
    for (int i = 0; i < 3; i++) {
        pylon_put_u16(w, kelvin(p->temp_avg_dc));
        pylon_put_u16(w, kelvin(p->temp_max_dc));
        pylon_put_u16(w, location(cfg, p->temp_max_module));
        pylon_put_u16(w, kelvin(p->temp_min_dc));
        pylon_put_u16(w, location(cfg, p->temp_min_module));
    }
}

static void cmd_62(pylon_writer_t *w, const pylon_data_t *d)
{
    const limits_out_t *l = d->lim;
    uint8_t a1 = 0, a2 = 0, p1 = 0, p2 = 0;
    a1 |= l->warn_cell_high ? 1u << 5 : 0;
    a1 |= l->warn_cell_low ? 1u << 4 : 0;
    a1 |= l->warn_temp_high ? 1u << 3 : 0;
    a1 |= l->warn_temp_low ? 1u << 2 : 0;
    a1 |= l->warn_imbalance ? 1u << 0 : 0;
    a2 |= l->fault_comm ? 1u << 4 : 0;
    p1 |= l->prot_cell_ov ? 1u << 5 : 0;
    p1 |= l->prot_cell_uv ? 1u << 4 : 0;
    p1 |= l->prot_temp_high ? 1u << 3 : 0;
    p1 |= l->prot_temp_low ? 1u << 2 : 0;
    pylon_put_u8(w, a1);
    pylon_put_u8(w, a2);
    pylon_put_u8(w, p1);
    pylon_put_u8(w, p2);
}

static void cmd_63(pylon_writer_t *w, const pylon_data_t *d)
{
    const limits_out_t *l = d->lim;
    pylon_put_u16(w, l->cvl_mv);
    pylon_put_u16(w, l->dvl_mv);
    pylon_put_u16(w, l->ccl_da);
    pylon_put_u16(w, l->dcl_da);
    pylon_put_u8(w, mgmt_status(l, 0x20));
}

size_t pylon_handle_request(const pylon_cfg_t *cfg, const pylon_data_t *d,
                            const char *req, size_t req_len,
                            char *out, size_t cap, pylon_req_info_t *info)
{
    pylon_frame_t f;
    int rc = pylon_frame_parse(req, req_len, &f);
    memset(info, 0, sizeof(*info));
    info->parse_rc = rc;
    if (rc == PYLON_PARSE_NOT_FRAME) {
        return 0;
    }
    info->ver = f.ver;
    info->adr = f.adr;
    info->cid2 = f.cid2;

    bool system = f.cid2 >= 0x60 && f.cid2 <= 0x64;
    info->for_us = f.adr == cfg->adr || (system && f.adr == cfg->sys_adr);
    if (!info->for_us) {
        return 0;   /* на шине может быть другая батарея */
    }

    uint8_t rtn = (uint8_t)rc;
    if (rc == PYLON_RTN_OK && f.cid1 != PYLON_CID1_BATTERY) {
        rtn = PYLON_RTN_FORMAT;
    }
    uint8_t ver = f.cid2 == 0x4F && rtn == PYLON_RTN_OK ? cfg->proto_ver : f.ver;
    uint8_t command = cfg->adr;
    if (rtn == PYLON_RTN_OK && f.lenid >= 2) {
        command = (uint8_t)pylon_hex_byte(f.info);
    }

    pylon_writer_t w;
    if (rtn == PYLON_RTN_OK) {
        switch (f.cid2) {
        case 0x42: case 0x44: case 0x47: case 0x4F: case 0x51:
        case 0x92: case 0x93: case 0x96: case 0x60: case 0x61: case 0x62: case 0x63:
            break;
        default:
            /* в т.ч. 0x94 «задать лимиты», 0x95 и 0x64 «выключить» — не выполняем */
            rtn = PYLON_RTN_CID2;
            break;
        }
    }
    pylon_writer_begin(&w, out, cap, ver, f.adr, PYLON_CID1_BATTERY, rtn);
    if (rtn == PYLON_RTN_OK) {
        switch (f.cid2) {
        case 0x42: cmd_42(&w, cfg, d, command); break;
        case 0x44: cmd_44(&w, d, command); break;
        case 0x47: cmd_47(&w, d); break;
        case 0x4F: break;
        case 0x51: cmd_51(&w, cfg); break;
        case 0x92: cmd_92(&w, cfg, d, command); break;
        case 0x93: cmd_93(&w, cfg, command); break;
        case 0x96: cmd_96(&w, cfg, command); break;
        case 0x60: cmd_60(&w, cfg); break;
        case 0x61: cmd_61(&w, cfg, d); break;
        case 0x62: cmd_62(&w, d); break;
        case 0x63: cmd_63(&w, d); break;
        default: break;
        }
    }
    info->rtn = rtn;
    return pylon_writer_end(&w);
}
