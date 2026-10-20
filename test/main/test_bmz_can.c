#include "unity.h"
#include "bmz_can.h"
#include "fixtures.h"

#define STALE 10000000LL

static void feed_basic(bmz_can_state_t *st, int64_t t)
{
    /* 0x351: CVL 56,0 В, CCL 25,0 А, DCL 30,0 А, DVL 47,0 В */
    const uint8_t f351[8] = {0x30, 0x02, 0xFA, 0x00, 0x2C, 0x01, 0xD6, 0x01};
    /* 0x355: SOC 75 %, SOH 98 % */
    const uint8_t f355[4] = {75, 0, 98, 0};
    /* 0x356: 53,20 В, −10,5 А, 25,1 °C */
    const uint8_t f356[6] = {0xC8, 0x14, 0x97, 0xFF, 0xFB, 0x00};
    /* 0x373: 3310…3330 мВ, 296…299 K */
    const uint8_t f373[8] = {0xEE, 0x0C, 0x02, 0x0D, 0x28, 0x01, 0x2B, 0x01};
    TEST_ASSERT_TRUE(bmz_can_feed(st, 0x351, f351, 8, t));
    TEST_ASSERT_TRUE(bmz_can_feed(st, 0x355, f355, 4, t));
    TEST_ASSERT_TRUE(bmz_can_feed(st, 0x356, f356, 6, t));
    TEST_ASSERT_TRUE(bmz_can_feed(st, 0x373, f373, 8, t));
}

static void test_decode_hypothesis_a(void)
{
    bmz_can_state_t st = {0};
    feed_basic(&st, NOW_US);
    pack_module_t m;
    bmz_can_to_module(&st, NOW_US, STALE, 1, &m);
    TEST_ASSERT_EQUAL_INT64(NOW_US, m.updated_us);
    TEST_ASSERT_EQUAL(53200, m.voltage_mv);
    TEST_ASSERT_EQUAL(-10500, m.current_ma);
    TEST_ASSERT_EQUAL(75, m.soc_pct);
    TEST_ASSERT_EQUAL(98, m.soh_pct);
    TEST_ASSERT_EQUAL(3310, m.cell_min_mv);
    TEST_ASSERT_EQUAL(3330, m.cell_max_mv);
    TEST_ASSERT_EQUAL(3, m.n_temps);
    TEST_ASSERT_EQUAL(251, m.temp_dc[0]);
    TEST_ASSERT_EQUAL(229, m.temp_dc[1]);    /* 296 K = 22,9 °C */
    TEST_ASSERT_TRUE(m.bms_limits_valid);
    TEST_ASSERT_EQUAL(56000, m.bms_cvl_mv);
    TEST_ASSERT_EQUAL(250, m.bms_ccl_da);
    TEST_ASSERT_EQUAL(300, m.bms_dcl_da);
    TEST_ASSERT_EQUAL(47000, m.bms_dvl_mv);
    TEST_ASSERT_TRUE(m.bms_charge_allowed && m.bms_discharge_allowed);

    /* Вся цепочка: BMS ограничила заряд 25 А — шлюз передаёт 25 А. */
    pack_summary_t p;
    limits_out_t l;
    pack_aggregate(&m, 1, NOW_US, &FX_PACK, &p);
    limits_compute(&FX_LIM, &p, &l);
    TEST_ASSERT_EQUAL(250, l.ccl_da);
    TEST_ASSERT_EQUAL(288, l.dcl_da);
}

static void test_flags_and_protection(void)
{
    bmz_can_state_t st = {0};
    feed_basic(&st, NOW_US);
    const uint8_t req_no_charge[1] = {0x40};
    TEST_ASSERT_TRUE(bmz_can_feed(&st, 0x35C, req_no_charge, 1, NOW_US));
    pack_module_t m;
    bmz_can_to_module(&st, NOW_US, STALE, 1, &m);
    TEST_ASSERT_FALSE(m.bms_charge_allowed);
    TEST_ASSERT_TRUE(m.bms_discharge_allowed);

    const uint8_t prot_uv[8] = {0x04, 0, 0, 0, 2, 'P', 'N', 0};
    TEST_ASSERT_TRUE(bmz_can_feed(&st, 0x359, prot_uv, 8, NOW_US));
    bmz_can_to_module(&st, NOW_US, STALE, 1, &m);
    TEST_ASSERT_FALSE(m.bms_discharge_allowed);
    TEST_ASSERT_EQUAL(2, m.units);           /* число модулей из 0x359 */
}

static void test_stale_parts_dropped(void)
{
    bmz_can_state_t st = {0};
    feed_basic(&st, NOW_US - 20000000);
    const uint8_t f356[6] = {0xC8, 0x14, 0x00, 0x00, 0xFA, 0x00};
    bmz_can_feed(&st, 0x356, f356, 6, NOW_US);
    pack_module_t m;
    bmz_can_to_module(&st, NOW_US, STALE, 1, &m);
    TEST_ASSERT_FALSE(m.bms_limits_valid);
    TEST_ASSERT_EQUAL(PACK_SOC_UNKNOWN, m.soc_pct);
    TEST_ASSERT_EQUAL(0, m.cell_max_mv);     /* без свежего 0x373 ячейки неизвестны */

    pack_summary_t p;
    limits_out_t l;
    pack_aggregate(&m, 1, NOW_US, &FX_PACK, &p);
    limits_compute(&FX_LIM, &p, &l);
    TEST_ASSERT_EQUAL(0, l.ccl_da);
    TEST_ASSERT_TRUE(l.reasons & LIM_NO_CELLS);
}

static void test_unknown_and_short_frames(void)
{
    bmz_can_state_t st = {0};
    const uint8_t d[8] = {0};
    TEST_ASSERT_FALSE(bmz_can_feed(&st, 0x305, d, 8, NOW_US));
    TEST_ASSERT_FALSE(bmz_can_feed(&st, 0x356, d, 4, NOW_US));
    TEST_ASSERT_EQUAL_INT64(0, st.t356);
}

void run_bmz_can_tests(void)
{
    RUN_TEST(test_decode_hypothesis_a);
    RUN_TEST(test_flags_and_protection);
    RUN_TEST(test_stale_parts_dropped);
    RUN_TEST(test_unknown_and_short_frames);
}
