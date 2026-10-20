#include <string.h>
#include "unity.h"
#include "fixtures.h"
#include "pylon_frame.h"

/* V3.5, 1.3.3: «1203400456ABCEFE» → CHKSUM FC71. */
static void test_checksum_spec_example(void)
{
    const char *s = "1203400456ABCEFE";
    TEST_ASSERT_EQUAL_HEX16(0xFC71, pylon_checksum(s, strlen(s)));
}

/* V3.5, 1.3.2: LENID = 18 → LENGTH D012. Остальные — из примеров V3.5. */
static void test_length_field(void)
{
    TEST_ASSERT_EQUAL_HEX16(0xD012, pylon_length_field(18));
    TEST_ASSERT_EQUAL_HEX16(0xE002, pylon_length_field(2));
    TEST_ASSERT_EQUAL_HEX16(0x8062, pylon_length_field(98));
    TEST_ASSERT_EQUAL_HEX16(0x8008, pylon_length_field(8));
    TEST_ASSERT_EQUAL_HEX16(0x0000, pylon_length_field(0));
    /* сумма полубайтов кратна 16: LCHKSUM = 0, а не 0x10 */
    TEST_ASSERT_EQUAL_HEX16(0x00F1, pylon_length_field(0x0F1));
}

/* Запрос 0x42 из ТЗ 4.2 / PowerForum. */
static void test_parse_request_42(void)
{
    const char *f = "~20024642E00202FD33\r";
    pylon_frame_t fr;
    TEST_ASSERT_EQUAL(PYLON_RTN_OK, pylon_frame_parse(f, strlen(f), &fr));
    TEST_ASSERT_EQUAL_HEX8(0x20, fr.ver);
    TEST_ASSERT_EQUAL_HEX8(0x02, fr.adr);
    TEST_ASSERT_EQUAL_HEX8(0x46, fr.cid1);
    TEST_ASSERT_EQUAL_HEX8(0x42, fr.cid2);
    TEST_ASSERT_EQUAL(2, fr.lenid);
    TEST_ASSERT_EQUAL_STRING_LEN("02", fr.info, 2);
}

/* V3.5, 2.2: запрос 0x61 мастеру 0x12. */
static void test_parse_v35_request_61(void)
{
    const char *f = "~201246610000FDAA\r";
    pylon_frame_t fr;
    TEST_ASSERT_EQUAL(PYLON_RTN_OK, pylon_frame_parse(f, strlen(f), &fr));
    TEST_ASSERT_EQUAL_HEX8(0x12, fr.adr);
    TEST_ASSERT_EQUAL_HEX8(0x61, fr.cid2);
    TEST_ASSERT_EQUAL(0, fr.lenid);
}

static void test_parse_errors(void)
{
    pylon_frame_t fr;
    const char *bad_chk = "~20024642E00202FD34\r";
    TEST_ASSERT_EQUAL(PYLON_RTN_CHKSUM, pylon_frame_parse(bad_chk, strlen(bad_chk), &fr));
    TEST_ASSERT_EQUAL_HEX8(0x42, fr.cid2);   /* заголовок разобран — можно ответить ошибкой */

    char buf[32];
    size_t n = fx_frame(buf, sizeof(buf), "~20024642F00202");   /* LCHKSUM F вместо E */
    TEST_ASSERT_EQUAL(PYLON_RTN_LCHKSUM, pylon_frame_parse(buf, n, &fr));

    n = fx_frame(buf, sizeof(buf), "~20024642C0040202");    /* LENID 4, INFO 4 — ок */
    TEST_ASSERT_EQUAL(PYLON_RTN_OK, pylon_frame_parse(buf, n, &fr));
    n = fx_frame(buf, sizeof(buf), "~20024642C00402");      /* LENID 4, а INFO 2 */
    TEST_ASSERT_EQUAL(PYLON_RTN_FORMAT, pylon_frame_parse(buf, n, &fr));

    const char *no_soi = "20024642E00202FD33\r";
    TEST_ASSERT_EQUAL(PYLON_PARSE_NOT_FRAME, pylon_frame_parse(no_soi, strlen(no_soi), &fr));
    const char *short_f = "~2002\r";
    TEST_ASSERT_EQUAL(PYLON_PARSE_NOT_FRAME, pylon_frame_parse(short_f, strlen(short_f), &fr));
    const char *not_hex = "~2X024642E00202FD33\r";
    TEST_ASSERT_EQUAL(PYLON_PARSE_NOT_FRAME, pylon_frame_parse(not_hex, strlen(not_hex), &fr));
}

/* V3.5, 2.2: ответ 0x61 из примера спецификации, байт в байт.
 * Поле «модуль» — 0x00GA (в таблице примера ошибочно 0x0304). */
static void test_writer_v35_response_61(void)
{
    static const uint16_t tail[] = {
        0x0DB8, 0x0034, 0x0CBB, 0x0014,
        0x0BAA, 0x0BB7, 0x0035, 0x0B9D, 0x0015,
        0x0BAA, 0x0BB8, 0x0036, 0x0B9C, 0x0016,
        0x0BAA, 0x0BB6, 0x0037, 0x0B9E, 0x0017,
    };
    char buf[160];
    pylon_writer_t w;
    pylon_writer_begin(&w, buf, sizeof(buf), 0x20, 0x12, 0x46, 0x00);
    pylon_put_u16(&w, 0x2E53);
    pylon_put_u16(&w, 0x61A8);
    pylon_put_u8(&w, 0x62);
    pylon_put_u16(&w, 0x09D4);
    pylon_put_u16(&w, 0x0B74);
    pylon_put_u8(&w, 0x62);
    pylon_put_u8(&w, 0x61);
    for (size_t i = 0; i < sizeof(tail) / sizeof(tail[0]); i++) {
        pylon_put_u16(&w, tail[i]);
    }
    size_t n = pylon_writer_end(&w);
    const char *expect =
        "~2012460080622E5361A86209D40B7462610DB800340CBB00140BAA0BB700350B9D0015"
        "0BAA0BB800360B9C00160BAA0BB600370B9E0017E862\r";
    TEST_ASSERT_EQUAL(strlen(expect), n);
    TEST_ASSERT_EQUAL_STRING_LEN(expect, buf, n);
}

/* V3.5, 2.3: ответ 0x62 без аварий. */
static void test_writer_v35_response_62(void)
{
    char buf[64];
    pylon_writer_t w;
    pylon_writer_begin(&w, buf, sizeof(buf), 0x20, 0x12, 0x46, 0x00);
    for (int i = 0; i < 4; i++) {
        pylon_put_u8(&w, 0);
    }
    size_t n = pylon_writer_end(&w);
    TEST_ASSERT_EQUAL_STRING_LEN("~20124600800800000000FC21\r", buf, n);
}

/* V3.3, раздел 4: ответ 0x42 батареи 74 А·ч. Раскладка и LENGTH сходятся,
 * а CHKSUM в примере ошибочен (E1A2, по сумме выходит E27A) — его не сравниваем. */
static void test_writer_v33_response_42_layout(void)
{
    static const uint16_t cells[15] = {
        0x0D45, 0x0D44, 0x0D45, 0x0D44, 0x0D45, 0x0D44, 0x0D3E, 0x0D45,
        0x0D4A, 0x0D4A, 0x0D4B, 0x0D4A, 0x0D4A, 0x0D4A, 0x0D4A,
    };
    static const uint16_t temps[5] = {0x0BC3, 0x0BC3, 0x0BC3, 0x0BCD, 0x0BCD};
    char buf[160];
    pylon_writer_t w;
    pylon_writer_begin(&w, buf, sizeof(buf), 0x20, 0x02, 0x46, 0x00);
    pylon_put_u8(&w, 0x11);
    pylon_put_u8(&w, 0x02);
    pylon_put_u8(&w, 15);
    for (int i = 0; i < 15; i++) {
        pylon_put_u16(&w, cells[i]);
    }
    pylon_put_u8(&w, 5);
    for (int i = 0; i < 5; i++) {
        pylon_put_u16(&w, temps[i]);
    }
    pylon_put_u16(&w, 0x0000);
    pylon_put_u16(&w, 0xC725);
    pylon_put_u16(&w, 0xFFFF);
    pylon_put_u8(&w, 4);
    pylon_put_u16(&w, 0xFFFF);
    pylon_put_u16(&w, 0x0002);
    pylon_put_u24(&w, 51800);
    pylon_put_u24(&w, 74000);
    size_t n = pylon_writer_end(&w);
    const char *expect =
        "~20024600F07A11020F0D450D440D450D440D450D440D3E0D450D4A0D4A0D4B0D4A0D4A0D4A0D4A"
        "050BC30BC30BC30BCD0BCD0000C725FFFF04FFFF000200CA58012110";
    TEST_ASSERT_EQUAL(strlen(expect) + 5, n);
    TEST_ASSERT_EQUAL_STRING_LEN(expect, buf, strlen(expect));
}

static void test_writer_text_and_overflow(void)
{
    char buf[64];
    pylon_writer_t w;
    pylon_writer_begin(&w, buf, sizeof(buf), 0x20, 0x02, 0x46, 0x00);
    pylon_put_text(&w, "Pylon", 6);
    size_t n = pylon_writer_end(&w);
    TEST_ASSERT_EQUAL_STRING_LEN("50796C6F6E00", buf + PYLON_HEADER_LEN, 12);
    pylon_frame_t fr;
    TEST_ASSERT_EQUAL(PYLON_RTN_OK, pylon_frame_parse(buf, n, &fr));

    char small[20];
    pylon_writer_begin(&w, small, sizeof(small), 0x20, 0x02, 0x46, 0x00);
    pylon_put_u16(&w, 0x1234);
    pylon_put_u16(&w, 0x5678);
    TEST_ASSERT_EQUAL(0, pylon_writer_end(&w));
}

void run_pylon_frame_tests(void)
{
    RUN_TEST(test_checksum_spec_example);
    RUN_TEST(test_length_field);
    RUN_TEST(test_parse_request_42);
    RUN_TEST(test_parse_v35_request_61);
    RUN_TEST(test_parse_errors);
    RUN_TEST(test_writer_v35_response_61);
    RUN_TEST(test_writer_v35_response_62);
    RUN_TEST(test_writer_v33_response_42_layout);
    RUN_TEST(test_writer_text_and_overflow);
}
