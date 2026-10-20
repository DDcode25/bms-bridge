/* Таблица 5.2 ТЗ и приёмочные T4, T6, T7, T9. */
#include "unity.h"
#include "fixtures.h"

static limits_out_t run(const pack_module_t *m, size_t n)
{
    pack_summary_t p;
    limits_out_t l;
    pack_aggregate(m, n, NOW_US, &FX_PACK, &p);
    limits_compute(&FX_LIM, &p, &l);
    return l;
}

static void test_normal(void)
{
    pack_module_t m = fx_module();
    limits_out_t l = run(&m, 1);
    TEST_ASSERT_EQUAL(288, l.ccl_da);
    TEST_ASSERT_EQUAL(288, l.dcl_da);
    TEST_ASSERT_EQUAL(56000, l.cvl_mv);
    TEST_ASSERT_EQUAL(48000, l.dvl_mv);
    TEST_ASSERT_TRUE(l.charge_enable);
    TEST_ASSERT_TRUE(l.discharge_enable);
    TEST_ASSERT_FALSE(l.charge_immediately);
    TEST_ASSERT_EQUAL_HEX32(0, l.reasons);
}

/* T4 / T8: нет данных (или устарели больше 10 с) — CCL = DCL = 0. */
static void test_no_or_stale_data(void)
{
    limits_out_t l = run(NULL, 0);
    TEST_ASSERT_EQUAL(0, l.ccl_da);
    TEST_ASSERT_EQUAL(0, l.dcl_da);
    TEST_ASSERT_FALSE(l.charge_enable || l.discharge_enable);
    TEST_ASSERT_EQUAL_HEX32(LIM_NO_DATA, l.reasons);

    pack_module_t m = fx_module();
    m.updated_us = NOW_US - 10500000;
    l = run(&m, 1);
    TEST_ASSERT_EQUAL(0, l.ccl_da);
    TEST_ASSERT_EQUAL_HEX32(LIM_NO_DATA, l.reasons);
}

/* T6: ячейка 3,56 В — CCL = 0. */
static void test_cell_overvoltage(void)
{
    pack_module_t m = fx_module();
    for (int i = 0; i < 16; i++) {
        m.cell_mv[i] = 3540;
    }
    m.cell_mv[7] = 3560;
    limits_out_t l = run(&m, 1);
    TEST_ASSERT_EQUAL(0, l.ccl_da);
    TEST_ASSERT_FALSE(l.charge_enable);
    TEST_ASSERT_TRUE(l.discharge_enable);
    TEST_ASSERT_TRUE(l.reasons & LIM_CELL_OV);
}

/* Выше 3,45 В — линейно до 0,1C на 3,55 В. */
static void test_cell_taper(void)
{
    pack_module_t m = fx_module();
    for (int i = 0; i < 16; i++) {
        m.cell_mv[i] = 3500;
    }
    limits_out_t l = run(&m, 1);
    TEST_ASSERT_EQUAL(58 + (288 - 58) / 2, l.ccl_da);
    TEST_ASSERT_TRUE(l.reasons & LIM_CELL_TAPER);

    for (int i = 0; i < 16; i++) {
        m.cell_mv[i] = 3550;
    }
    l = run(&m, 1);
    TEST_ASSERT_EQUAL(58, l.ccl_da);
}

static void test_cell_undervoltage(void)
{
    pack_module_t m = fx_module();
    m.cell_mv[0] = 2880;
    for (int i = 1; i < 16; i++) {
        m.cell_mv[i] = 2920;
    }
    limits_out_t l = run(&m, 1);
    TEST_ASSERT_EQUAL(0, l.dcl_da);
    TEST_ASSERT_EQUAL(288, l.ccl_da);
    TEST_ASSERT_TRUE(l.charge_immediately);
    TEST_ASSERT_TRUE(l.reasons & LIM_CELL_UV);
}

static void set_temp(pack_module_t *m, int16_t dc)
{
    for (int i = 0; i < m->n_temps; i++) {
        m->temp_dc[i] = dc;
    }
}

/* T7: 0 °C — CCL = 0; разряд разрешён. */
static void test_temperature(void)
{
    pack_module_t m = fx_module();
    set_temp(&m, 0);
    limits_out_t l = run(&m, 1);
    TEST_ASSERT_EQUAL(0, l.ccl_da);
    TEST_ASSERT_EQUAL(288, l.dcl_da);

    set_temp(&m, 460);
    l = run(&m, 1);
    TEST_ASSERT_EQUAL(144, l.ccl_da);
    TEST_ASSERT_EQUAL(144, l.dcl_da);

    set_temp(&m, 560);
    l = run(&m, 1);
    TEST_ASSERT_EQUAL(0, l.ccl_da);
    TEST_ASSERT_EQUAL(0, l.dcl_da);
    TEST_ASSERT_TRUE(l.reasons & LIM_TEMP_HIGH);
}

static void test_imbalance(void)
{
    pack_module_t m = fx_module();
    for (int i = 0; i < 16; i++) {
        m.cell_mv[i] = 3360;
    }
    m.cell_mv[2] = 3420;
    limits_out_t l = run(&m, 1);
    TEST_ASSERT_EQUAL(58, l.ccl_da);   /* верхний участок — до 0,1C */
    TEST_ASSERT_TRUE(l.warn_imbalance);

    m.cell_mv[2] = 3300;
    m.cell_mv[3] = 3240;               /* разбаланс внизу — CCL не трогаем */
    l = run(&m, 1);
    TEST_ASSERT_EQUAL(288, l.ccl_da);
    TEST_ASSERT_TRUE(l.warn_imbalance);
}

/* T9: из двух модулей выпал один — CCL/DCL вдвое. */
static void test_module_dropout(void)
{
    pack_module_t m[2] = {fx_module(), fx_module()};
    limits_out_t l = run(m, 2);
    TEST_ASSERT_EQUAL(576, l.ccl_da);
    m[1].updated_us = NOW_US - 11000000;
    l = run(m, 2);
    TEST_ASSERT_EQUAL(288, l.ccl_da);
    TEST_ASSERT_EQUAL(288, l.dcl_da);
}

static void test_bms_limits_and_blocks(void)
{
    pack_module_t m = fx_module();
    m.bms_limits_valid = true;
    m.bms_cvl_mv = 55200;
    m.bms_dvl_mv = 49000;
    m.bms_ccl_da = 150;
    m.bms_dcl_da = 400;
    limits_out_t l = run(&m, 1);
    TEST_ASSERT_EQUAL(150, l.ccl_da);
    TEST_ASSERT_EQUAL(288, l.dcl_da);
    TEST_ASSERT_EQUAL(55200, l.cvl_mv);
    TEST_ASSERT_EQUAL(49000, l.dvl_mv);

    m.bms_discharge_allowed = false;
    l = run(&m, 1);
    TEST_ASSERT_EQUAL(0, l.dcl_da);
    TEST_ASSERT_TRUE(l.reasons & LIM_BMS_NO_DISCHG);
}

/* Неизвестные ячейки или температура — тоже неопределённость. */
static void test_missing_cells_or_temps(void)
{
    pack_module_t m = fx_module();
    m.n_cells = 0;
    m.cell_min_mv = 0;
    m.cell_max_mv = 0;
    limits_out_t l = run(&m, 1);
    TEST_ASSERT_EQUAL(0, l.ccl_da);
    TEST_ASSERT_EQUAL(0, l.dcl_da);
    TEST_ASSERT_TRUE(l.reasons & LIM_NO_CELLS);

    m = fx_module();
    m.n_temps = 0;
    l = run(&m, 1);
    TEST_ASSERT_EQUAL(0, l.ccl_da);
    TEST_ASSERT_TRUE(l.reasons & LIM_NO_TEMP);
}

static void test_low_soc_requests_charge(void)
{
    pack_module_t m = fx_module();
    m.soc_pct = 3;
    limits_out_t l = run(&m, 1);
    TEST_ASSERT_TRUE(l.charge_immediately);
    TEST_ASSERT_TRUE(l.discharge_enable);
}

void run_limits_tests(void)
{
    RUN_TEST(test_normal);
    RUN_TEST(test_no_or_stale_data);
    RUN_TEST(test_cell_overvoltage);
    RUN_TEST(test_cell_taper);
    RUN_TEST(test_cell_undervoltage);
    RUN_TEST(test_temperature);
    RUN_TEST(test_imbalance);
    RUN_TEST(test_module_dropout);
    RUN_TEST(test_bms_limits_and_blocks);
    RUN_TEST(test_missing_cells_or_temps);
    RUN_TEST(test_low_soc_requests_charge);
}
