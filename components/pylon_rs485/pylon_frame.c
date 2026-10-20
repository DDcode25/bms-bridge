#include <string.h>
#include "pylon_frame.h"

static const char HEX[] = "0123456789ABCDEF";

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

int pylon_hex_byte(const char *p)
{
    int hi = hex_nibble(p[0]);
    int lo = hex_nibble(p[1]);
    return (hi < 0 || lo < 0) ? -1 : (hi << 4) | lo;
}

static int hex_u16(const char *p)
{
    int hi = pylon_hex_byte(p);
    int lo = pylon_hex_byte(p + 2);
    return (hi < 0 || lo < 0) ? -1 : (hi << 8) | lo;
}

uint16_t pylon_checksum(const char *s, size_t n)
{
    uint16_t sum = 0;
    for (size_t i = 0; i < n; i++) {
        sum += (uint8_t)s[i];   /* mod 65536 за счёт переполнения uint16_t */
    }
    return (uint16_t)(~sum + 1);
}

uint16_t pylon_length_field(uint16_t lenid)
{
    lenid &= 0x0FFF;
    unsigned sum = (lenid & 0xF) + ((lenid >> 4) & 0xF) + ((lenid >> 8) & 0xF);
    unsigned lchk = (~sum + 1) & 0xF;
    return (uint16_t)((lchk << 12) | lenid);
}

int pylon_frame_parse(const char *buf, size_t len, pylon_frame_t *f)
{
    memset(f, 0, sizeof(*f));
    if (len < PYLON_MIN_FRAME || buf[0] != PYLON_SOI || buf[len - 1] != PYLON_EOI) {
        return PYLON_PARSE_NOT_FRAME;
    }
    int ver = pylon_hex_byte(buf + 1);
    int adr = pylon_hex_byte(buf + 3);
    int cid1 = pylon_hex_byte(buf + 5);
    int cid2 = pylon_hex_byte(buf + 7);
    int length = hex_u16(buf + 9);
    int chk = hex_u16(buf + len - 5);
    if (ver < 0 || adr < 0 || cid1 < 0 || cid2 < 0 || length < 0 || chk < 0) {
        return PYLON_PARSE_NOT_FRAME;
    }
    f->ver = (uint8_t)ver;
    f->adr = (uint8_t)adr;
    f->cid1 = (uint8_t)cid1;
    f->cid2 = (uint8_t)cid2;
    f->lenid = (uint16_t)(length & 0x0FFF);
    f->info = buf + PYLON_HEADER_LEN;

    if (pylon_checksum(buf + 1, len - 6) != (uint16_t)chk) {
        return PYLON_RTN_CHKSUM;
    }
    if (pylon_length_field(f->lenid) != (uint16_t)length) {
        return PYLON_RTN_LCHKSUM;
    }
    if ((size_t)f->lenid != len - PYLON_MIN_FRAME || (f->lenid & 1)) {
        return PYLON_RTN_FORMAT;
    }
    for (size_t i = 0; i < f->lenid; i++) {
        if (hex_nibble(f->info[i]) < 0) {
            return PYLON_RTN_FORMAT;
        }
    }
    return PYLON_RTN_OK;
}

static void put_hex(pylon_writer_t *w, uint32_t v, int digits)
{
    if (w->len + (size_t)digits > w->cap) {
        w->overflow = true;
        return;
    }
    for (int i = digits - 1; i >= 0; i--) {
        w->buf[w->len++] = HEX[(v >> (i * 4)) & 0xF];
    }
}

void pylon_writer_begin(pylon_writer_t *w, char *buf, size_t cap,
                        uint8_t ver, uint8_t adr, uint8_t cid1, uint8_t cid2)
{
    w->buf = buf;
    w->cap = cap;
    w->len = 0;
    w->overflow = cap < PYLON_MIN_FRAME;
    if (w->overflow) {
        return;
    }
    w->buf[w->len++] = PYLON_SOI;
    put_hex(w, ver, 2);
    put_hex(w, adr, 2);
    put_hex(w, cid1, 2);
    put_hex(w, cid2, 2);
    put_hex(w, 0, 4);   /* LENGTH, заполняется в pylon_writer_end */
}

void pylon_put_u8(pylon_writer_t *w, uint8_t v) { put_hex(w, v, 2); }
void pylon_put_u16(pylon_writer_t *w, uint16_t v) { put_hex(w, v, 4); }
void pylon_put_u24(pylon_writer_t *w, uint32_t v) { put_hex(w, v & 0xFFFFFF, 6); }

void pylon_put_text(pylon_writer_t *w, const char *s, size_t width)
{
    size_t n = s ? strlen(s) : 0;
    for (size_t i = 0; i < width; i++) {
        put_hex(w, i < n ? (uint8_t)s[i] : 0, 2);
    }
}

size_t pylon_writer_end(pylon_writer_t *w)
{
    if (w->overflow || w->len + 5 > w->cap) {
        return 0;
    }
    size_t lenid = w->len - PYLON_HEADER_LEN;
    if (lenid > 0x0FFF) {
        return 0;
    }
    size_t save = w->len;
    w->len = 9;
    put_hex(w, pylon_length_field((uint16_t)lenid), 4);
    w->len = save;
    put_hex(w, pylon_checksum(w->buf + 1, w->len - 1), 4);
    w->buf[w->len++] = PYLON_EOI;
    return w->len;
}
