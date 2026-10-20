#include "unity.h"
#include "fixtures.h"

static void test_two_modules(void)
{
    pack_module_t m[2] = {fx_module(), fx_module()};
    m[1].voltage_mv = 52600;
    m[1].current_ma = 5000;
    m[1].soc_pct = 40;
    m[1].capacity_mah = 28800;
    m[1].cell_mv[5] = 3350;
    m[1].temp_dc[2] = 310;
    pack_summary_t s;
    pack_aggregate(m, 2, NOW_US, &FX_PACK, &s);
    TEST_ASSERT_EQUAL(2, s.modules_ok);
    TEST_ASSERT_EQUAL(52700, s.voltage_mv);
    TEST_ASSERT_EQUAL(15000, s.current_ma);
    TEST_ASSERT_EQUAL(86400, s.capacity_mah);
    TEST_ASSERT_EQUAL(53, s.soc_pct);           /* (57,6×60 + 28,8×40) / 86,4 */
    TEST_ASSERT_EQUAL(3350, s.cell_max_mv);
    TEST_ASSERT_EQUAL(1, s.cell_max_module);
    TEST_ASSERT_EQUAL(3350, s.cell_mv[5]);      /* заряд — худшая = максимальная */
    TEST_ASSERT_EQUAL(310, s.temp_max_dc);
    TEST_ASSERT_FALSE(s.cells_synthetic);
}

static void test_discharge_takes_min_cells(void)
{
    pack_module_t m[2] = {fx_module(), fx_module()};
    m[0].current_ma = -20000;
    m[1].current_ma = -20000;
    m[1].cell_mv[4] = 3100;
    pack_summary_t s;
    pack_aggregate(m, 2, NOW_US, &FX_PACK, &s);
    TEST_ASSERT_EQUAL(3100, s.cell_mv[4]);
    TEST_ASSERT_EQUAL(3100, s.cell_min_mv);
}

static void test_min_max_only_cells(void)
{
    pack_module_t m = fx_module();
    m.n_cells = 0;
    m.cell_min_mv = 3280;
    m.cell_max_mv = 3320;
    pack_summary_t s;
    pack_aggregate(&m, 1, NOW_US, &FX_PACK, &s);
    TEST_ASSERT_TRUE(s.cells_valid);
    TEST_ASSERT_TRUE(s.cells_synthetic);
    TEST_ASSERT_EQUAL(3320, s.cell_mv[0]);
    TEST_ASSERT_EQUAL(3280, s.cell_mv[1]);
    TEST_ASSERT_EQUAL(3300, s.cell_mv[2]);
}

static void test_unknown_soc_and_capacity_default(void)
{
    pack_module_t m = fx_module();
    m.soc_pct = PACK_SOC_UNKNOWN;
    m.capacity_mah = 0;
    m.units = 3;
    pack_summary_t s;
    pack_aggregate(&m, 1, NOW_US, &FX_PACK, &s);
    TEST_ASSERT_FALSE(s.soc_valid);
    TEST_ASSERT_EQUAL(3, s.units_ok);
    TEST_ASSERT_EQUAL(3 * 57600, s.capacity_mah);
}

static void test_newest_kept_when_all_stale(void)
{
    pack_module_t m = fx_module();
    m.updated_us = NOW_US - 30000000;
    pack_summary_t s;
    pack_aggregate(&m, 1, NOW_US, &FX_PACK, &s);
    TEST_ASSERT_EQUAL(0, s.modules_ok);
    TEST_ASSERT_EQUAL_INT64(NOW_US - 30000000, s.newest_us);  /* нужен для «молчать после 60 с» */
}

void run_pack_tests(void)
{
    RUN_TEST(test_two_modules);
    RUN_TEST(test_discharge_takes_min_cells);
    RUN_TEST(test_min_max_only_cells);
    RUN_TEST(test_unknown_soc_and_capacity_default);
    RUN_TEST(test_newest_kept_when_all_stale);
}
