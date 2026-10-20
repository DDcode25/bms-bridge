#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "fixtures.h"
#include "pylon_cmd.h"
#include "pylon_frame.h"

static pylon_cfg_t cfg(void)
{
    return (pylon_cfg_t){
        .adr = 0x02,
        .sys_adr = 0x12,
        .proto_ver = 0x20,
        .current_unit_ma = 100,
        .dcl_negative = false,
        .device_name = "BMZ618031",
        .manufacturer = "PYLON",
        .sw_version = 0x0100,
        .serial = "BMSBRIDGE0000001",
    };
}

typedef struct {
    char resp[512];
    size_t len;
    pylon_frame_t fr;
    int rc;
    pylon_req_info_t info;
} reply_t;

/* Запрос VER 0x20 к adr с INFO info; ответ разбирается обратно в r. */
static void ask_cfg(reply_t *r, const pylon_cfg_t *c, const pack_summary_t *p, uint8_t adr,
                    uint8_t cid2, const char *info)
{
    limits_out_t lim;
    limits_compute(&FX_LIM, p, &lim);
    pylon_data_t d = {.pack = p, .lim = &lim, .lim_cfg = &FX_LIM};

    char body[64], req[72];
    snprintf(body, sizeof(body), "~20%02X46%02X%04X%s", adr, cid2,
             pylon_length_field((uint16_t)strlen(info)), info);
    size_t n = fx_frame(req, sizeof(req), body);

    memset(r, 0, sizeof(*r));
    r->len = pylon_handle_request(c, &d, req, n, r->resp, sizeof(r->resp), &r->info);
    r->rc = r->len ? pylon_frame_parse(r->resp, r->len, &r->fr) : PYLON_PARSE_NOT_FRAME;
}

static void ask(reply_t *r, const pack_summary_t *p, uint8_t adr, uint8_t cid2, const char *info)
{
    pylon_cfg_t c = cfg();
    ask_cfg(r, &c, p, adr, cid2, info);
}

#define ASSERT_INFO(expect, r)                                          \
    do {                                                                \
        TEST_ASSERT_EQUAL(PYLON_RTN_OK, (r).rc);                        \
        TEST_ASSERT_EQUAL_HEX8(PYLON_RTN_OK, (r).fr.cid2);              \
        TEST_ASSERT_EQUAL(strlen(expect), (r).fr.lenid);                \
        TEST_ASSERT_EQUAL_STRING_LEN((expect), (r).fr.info, (r).fr.lenid); \
    } while (0)

static void test_4f_protocol_version(void)
{
    pack_summary_t p = fx_summary(1);
    reply_t r;
    ask(&r, &p, 0x02, 0x4F, "");
    TEST_ASSERT_EQUAL(PYLON_RTN_OK, r.rc);
    TEST_ASSERT_EQUAL_HEX8(0x20, r.fr.ver);
    TEST_ASSERT_EQUAL_HEX8(0x02, r.fr.adr);
    TEST_ASSERT_EQUAL_HEX8(0x00, r.fr.cid2);
    TEST_ASSERT_EQUAL(0, r.fr.lenid);
}

static void test_42_analog_single_module(void)
{
    pack_summary_t p = fx_summary(1);
    reply_t r;
    ask(&r, &p, 0x02, 0x42, "02");
    char expect[256] = "000210";
    for (int i = 0; i < 16; i++) {
        strcat(expect, "0CE4");                 /* 3300 мВ */
    }
    strcat(expect, "05");
    for (int i = 0; i < 5; i++) {
        strcat(expect, "0BA5");                 /* 25,0 °C = 2981 (0,1 K) */
    }
    strcat(expect, "0064"                       /* +10 А в шагах 100 мА */
                   "CE40"                       /* 52800 мВ */
                   "8700" "02" "E100"           /* 34,56 / 57,6 А·ч */
                   "000A");                     /* циклы */
    ASSERT_INFO(expect, r);
}

/* Больше 65 А·ч: USER DEFINED = 4 и 24-битные ёмкости (V3.2). */
static void test_42_big_capacity(void)
{
    pack_summary_t p = fx_summary(5);
    reply_t r;
    ask(&r, &p, 0x02, 0x42, "02");
    TEST_ASSERT_EQUAL(PYLON_RTN_OK, r.rc);
    /* ток 50 А = 500 = 0x01F4, U, FFFF, 04, FFFF, циклы, 172800, 288000 мА·ч */
    TEST_ASSERT_EQUAL_STRING_LEN("01F4CE40FFFF04FFFF000A02A300046500",
                                 r.fr.info + r.fr.lenid - 34, 34);
}

static void test_42_all_modules_request(void)
{
    pack_summary_t p = fx_summary(1);
    reply_t r;
    ask(&r, &p, 0x02, 0x42, "FF");
    TEST_ASSERT_EQUAL(PYLON_RTN_OK, r.rc);
    TEST_ASSERT_EQUAL_STRING_LEN("000110", r.fr.info, 6);   /* флаг, 1 модуль, 16 ячеек */
}

static void test_92_management(void)
{
    pack_summary_t p = fx_summary(1);
    reply_t r;
    ask(&r, &p, 0x02, 0x92, "02");
    /* CVL 56000, DVL 48000 мВ, CCL = DCL = 28,8 А, заряд и разряд разрешены */
    ASSERT_INFO("02DAC0BB8001200120C0", r);
}

static void test_92_dcl_negative(void)
{
    pack_summary_t p = fx_summary(1);
    pylon_cfg_t c = cfg();
    c.dcl_negative = true;
    reply_t r;
    ask_cfg(&r, &c, &p, 0x02, 0x92, "02");
    ASSERT_INFO("02DAC0BB800120FEE0C0", r);
}

/* ТЗ 5.2: после перезагрузки до данных BMZ — CCL = DCL = 0, флаги сняты. */
static void test_92_no_data(void)
{
    pack_summary_t p;
    pack_aggregate(NULL, 0, NOW_US, &FX_PACK, &p);
    reply_t r;
    ask(&r, &p, 0x02, 0x92, "02");
    ASSERT_INFO("02DAC0BB800000000000", r);
}

static void test_63_system_management(void)
{
    pack_summary_t p = fx_summary(2);
    reply_t r;
    ask(&r, &p, 0x12, 0x63, "");
    TEST_ASSERT_EQUAL_HEX8(0x12, r.fr.adr);
    ASSERT_INFO("DAC0BB8002400240C0", r);     /* 2 модуля: 57,6 А */
}

static void test_61_system_analog(void)
{
    pack_summary_t p = fx_summary(1);
    reply_t r;
    ask(&r, &p, 0x12, 0x61, "");
    TEST_ASSERT_EQUAL(PYLON_RTN_OK, r.rc);
    TEST_ASSERT_EQUAL(98, r.fr.lenid);          /* 49 байт, как в примере V3.5 */
    /* U 52800 мВ, I 10 А в 10 мА = 1000, SOC 60, циклы, SOH, max ячейка */
    TEST_ASSERT_EQUAL_STRING_LEN("CE4003E83C000A000A64640CE4" "0012", r.fr.info, 30);  /* группа 1, адрес 2 */
}

static void test_62_alarms(void)
{
    pack_module_t m = fx_module();
    m.cell_mv[3] = 3560;
    pack_summary_t p;
    pack_aggregate(&m, 1, NOW_US, &FX_PACK, &p);
    reply_t r;
    ask(&r, &p, 0x12, 0x62, "");
    /* разбаланс 260 мВ (бит 0) + защита по перенапряжению ячейки (бит 5) */
    ASSERT_INFO("01002000", r);
}

/* V3.3, 3.6: «charge immediately» — оба бита 5 и 4; V3.5 0x63 — только бит 5. */
static void test_charge_immediately_bits(void)
{
    pack_module_t m = fx_module();
    m.soc_pct = 3;
    pack_summary_t p;
    pack_aggregate(&m, 1, NOW_US, &FX_PACK, &p);
    reply_t r;
    ask(&r, &p, 0x02, 0x92, "02");
    TEST_ASSERT_EQUAL_STRING_LEN("F0", r.fr.info + 18, 2);
    ask(&r, &p, 0x12, 0x63, "");
    TEST_ASSERT_EQUAL_STRING_LEN("E0", r.fr.info + 16, 2);
}

static void test_44_alarm_info(void)
{
    pack_summary_t p = fx_summary(1);
    reply_t r;
    ask(&r, &p, 0x02, 0x44, "02");
    char expect[128] = "000210";
    for (int i = 0; i < 16; i++) {
        strcat(expect, "00");
    }
    strcat(expect, "05" "0000000000"
                   "000000"     /* ток заряда, напряжение модуля, ток разряда */
                   "00"         /* Status 1 */
                   "0E"         /* Status 2: питание от модуля, MOSFET разряда и заряда */
                   "80"         /* Status 3: идёт заряд > 0,1 А */
                   "0000");     /* Status 4/5 */
    ASSERT_INFO(expect, r);

    pack_module_t m = fx_module();
    m.cell_mv[2] = 3560;
    m.cell_mv[9] = 4300;        /* > 4,2 В — «неисправная» ячейка */
    pack_aggregate(&m, 1, NOW_US, &FX_PACK, &p);
    ask(&r, &p, 0x02, 0x44, "02");
    TEST_ASSERT_EQUAL(PYLON_RTN_OK, r.rc);
    TEST_ASSERT_EQUAL_STRING_LEN("10", r.fr.info, 2);           /* INFOFLAG: есть аварии */
    TEST_ASSERT_EQUAL_STRING_LEN("02", r.fr.info + 6 + 2 * 2, 2);  /* ячейка 3 выше порога */
    TEST_ASSERT_EQUAL_STRING_LEN("0002", r.fr.info + r.fr.lenid - 4, 4); /* Status 5, бит 1 */
    /* заряд запрещён — MOSFET заряда «выключен» в Status 2 */
    TEST_ASSERT_EQUAL_STRING_LEN("0C", r.fr.info + r.fr.lenid - 8, 2);
}

static void test_96_software_version(void)
{
    pack_summary_t p = fx_summary(1);
    reply_t r;
    ask(&r, &p, 0x02, 0x96, "02");
    ASSERT_INFO("020100000100", r);
}

static void test_51_and_93_text(void)
{
    pack_summary_t p = fx_summary(1);
    reply_t r;
    ask(&r, &p, 0x02, 0x51, "");
    TEST_ASSERT_EQUAL(PYLON_RTN_OK, r.rc);
    TEST_ASSERT_EQUAL(64, r.fr.lenid);          /* 10 + 2 + 20 байт */
    TEST_ASSERT_EQUAL_STRING_LEN("424D5A363138303331000100", r.fr.info, 24);

    ask(&r, &p, 0x02, 0x93, "02");
    TEST_ASSERT_EQUAL(34, r.fr.lenid);
    TEST_ASSERT_EQUAL_STRING_LEN("02424D53", r.fr.info, 8);
}

static void test_unknown_and_shutdown_rejected(void)
{
    pack_summary_t p = fx_summary(1);
    reply_t r;
    ask(&r, &p, 0x12, 0x64, "");        /* «выключить батарею» не выполняем */
    TEST_ASSERT_EQUAL(PYLON_RTN_OK, r.rc);
    TEST_ASSERT_EQUAL_HEX8(PYLON_RTN_CID2, r.fr.cid2);
    ask(&r, &p, 0x02, 0x99, "");
    TEST_ASSERT_EQUAL_HEX8(PYLON_RTN_CID2, r.fr.cid2);
    ask(&r, &p, 0x02, 0x94, "02DAC0BB8001200120");   /* «задать лимиты» */
    TEST_ASSERT_EQUAL_HEX8(PYLON_RTN_CID2, r.fr.cid2);
    ask(&r, &p, 0x02, 0x95, "02");                   /* «выключить модуль» */
    TEST_ASSERT_EQUAL_HEX8(PYLON_RTN_CID2, r.fr.cid2);
}

static void test_addressing(void)
{
    pack_summary_t p = fx_summary(1);
    reply_t r;
    ask(&r, &p, 0x03, 0x42, "03");      /* другая батарея на шине */
    TEST_ASSERT_EQUAL(0, r.len);
    TEST_ASSERT_FALSE(r.info.for_us);
    ask(&r, &p, 0x12, 0x42, "12");              /* классические команды — только на ADR */
    TEST_ASSERT_EQUAL(0, r.len);
    ask(&r, &p, 0x02, 0x63, "");                /* системные — и на ADR тоже */
    TEST_ASSERT_EQUAL(PYLON_RTN_OK, r.rc);
    TEST_ASSERT_EQUAL_HEX8(0x02, r.fr.adr);
}

static void test_bad_checksum_answered_with_rtn(void)
{
    pack_summary_t p = fx_summary(1);
    limits_out_t lim;
    limits_compute(&FX_LIM, &p, &lim);
    pylon_data_t d = {.pack = &p, .lim = &lim, .lim_cfg = &FX_LIM};
    pylon_cfg_t c = cfg();
    const char *req = "~20024642E00202FD34\r";
    char resp[64];
    pylon_req_info_t info;
    size_t n = pylon_handle_request(&c, &d, req, strlen(req), resp, sizeof(resp), &info);
    pylon_frame_t fr;
    TEST_ASSERT_EQUAL(PYLON_RTN_OK, pylon_frame_parse(resp, n, &fr));
    TEST_ASSERT_EQUAL_HEX8(PYLON_RTN_CHKSUM, fr.cid2);
    TEST_ASSERT_EQUAL(0, fr.lenid);
}

void run_pylon_cmd_tests(void)
{
    RUN_TEST(test_4f_protocol_version);
    RUN_TEST(test_42_analog_single_module);
    RUN_TEST(test_42_big_capacity);
    RUN_TEST(test_42_all_modules_request);
    RUN_TEST(test_92_management);
    RUN_TEST(test_92_dcl_negative);
    RUN_TEST(test_92_no_data);
    RUN_TEST(test_63_system_management);
    RUN_TEST(test_61_system_analog);
    RUN_TEST(test_62_alarms);
    RUN_TEST(test_charge_immediately_bits);
    RUN_TEST(test_44_alarm_info);
    RUN_TEST(test_96_software_version);
    RUN_TEST(test_51_and_93_text);
    RUN_TEST(test_unknown_and_shutdown_rejected);
    RUN_TEST(test_addressing);
    RUN_TEST(test_bad_checksum_answered_with_rtn);
}
