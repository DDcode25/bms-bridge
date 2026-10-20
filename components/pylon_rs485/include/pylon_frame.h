/*
 * Кадр Pylontech RS485 (PYLON low voltage RS485 V3.5, раздел 1.2–1.3).
 *
 *   SOI VER ADR CID1 CID2 LENGTH INFO CHKSUM EOI
 *   '~'  2    2    2    2     4    N     4    '\r'   (символы ASCII-hex)
 *
 * LENGTH: старший полубайт — LCHKSUM, младшие 12 бит — LENID (число
 * ASCII-символов INFO). LCHKSUM = (~(сумма трёх полубайтов LENID mod 16) + 1) & 0xF.
 * CHKSUM = ~(сумма кодов символов между SOI и CHKSUM mod 65536) + 1.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PYLON_SOI '~'
#define PYLON_EOI '\r'
#define PYLON_HEADER_LEN 13   /* SOI + VER ADR CID1 CID2 + LENGTH */
#define PYLON_MIN_FRAME  18   /* заголовок + CHKSUM + EOI, INFO пустой */
#define PYLON_CID1_BATTERY 0x46

/* Коды возврата в поле CID2 ответа (V3.5, 1.5.2). */
enum {
    PYLON_RTN_OK          = 0x00,
    PYLON_RTN_VER         = 0x01,
    PYLON_RTN_CHKSUM      = 0x02,
    PYLON_RTN_LCHKSUM     = 0x03,
    PYLON_RTN_CID2        = 0x04,
    PYLON_RTN_FORMAT      = 0x05,
    PYLON_RTN_INVALID     = 0x06,
    PYLON_RTN_ADR         = 0x90,
    PYLON_RTN_COMM        = 0x91,
};

#define PYLON_PARSE_NOT_FRAME (-1)

typedef struct {
    uint8_t ver;
    uint8_t adr;
    uint8_t cid1;
    uint8_t cid2;
    uint16_t lenid;
    const char *info;   /* указывает внутрь буфера запроса, ASCII-hex */
} pylon_frame_t;

uint16_t pylon_checksum(const char *s, size_t n);
uint16_t pylon_length_field(uint16_t lenid);
int pylon_hex_byte(const char *p);  /* 0..255 или -1 */

/* Разбор кадра целиком, от '~' до '\r' включительно.
 * Возвращает PYLON_RTN_OK, код ошибки PYLON_RTN_* (заголовок при этом
 * разобран и на кадр можно ответить ошибкой) или PYLON_PARSE_NOT_FRAME. */
int pylon_frame_parse(const char *buf, size_t len, pylon_frame_t *out);

/* Сборка ответа. */
typedef struct {
    char *buf;
    size_t cap;
    size_t len;
    bool overflow;
} pylon_writer_t;

void pylon_writer_begin(pylon_writer_t *w, char *buf, size_t cap,
                        uint8_t ver, uint8_t adr, uint8_t cid1, uint8_t cid2);
void pylon_put_u8(pylon_writer_t *w, uint8_t v);
void pylon_put_u16(pylon_writer_t *w, uint16_t v);
void pylon_put_u24(pylon_writer_t *w, uint32_t v);
/* Строка фиксированной ширины в байтах, дополняется 0x00 (V3.5, пример 2.1). */
void pylon_put_text(pylon_writer_t *w, const char *s, size_t width);
/* Дописывает LENGTH, CHKSUM и EOI. Возвращает длину кадра или 0 при переполнении. */
size_t pylon_writer_end(pylon_writer_t *w);

#ifdef __cplusplus
}
#endif
