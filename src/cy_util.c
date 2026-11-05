/* ColorfulYomi: 文字列・メモリ・UTF-8 の小道具  SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <string.h>
#include "cy_internal.h"

struct CyBlock { CyBlock* next; size_t used, cap; };
#define BLOCK_DATA(b) ((char*)((b) + 1))

void* cy_alloc(CyArena* a, size_t n)
{
    CyBlock* b = a->head;
    n = (n + 7) & ~(size_t)7;
    if (!b || b->cap - b->used < n) {
        size_t cap = n > 32768 ? n : 32768;
        b = (CyBlock*)malloc(sizeof(CyBlock) + cap);
        if (!b) { a->oom = 1; return NULL; }
        b->next = a->head; b->used = 0; b->cap = cap;
        a->head = b;
    }
    b->used += n;
    return BLOCK_DATA(b) + b->used - n;
}

char* cy_strndup(CyArena* a, const char* s, size_t n)
{
    char* p = (char*)cy_alloc(a, n + 1);
    if (!p) return NULL;
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

char* cy_strdup(CyArena* a, const char* s) { return cy_strndup(a, s, strlen(s)); }

void cy_arena_free(CyArena* a)
{
    while (a->head) { CyBlock* n = a->head->next; free(a->head); a->head = n; }
}

void cy_str_putn(CyStr* s, const char* p, size_t n)
{
    if (s->oom) return;
    if (s->n + n + 1 > s->cap) {
        size_t cap = s->cap ? s->cap * 2 : 256;
        char* q;
        while (cap < s->n + n + 1) cap *= 2;
        q = (char*)realloc(s->p, cap);
        if (!q) { s->oom = 1; return; }
        s->p = q; s->cap = cap;
    }
    memcpy(s->p + s->n, p, n);
    s->n += n;
    s->p[s->n] = '\0';
}

void cy_str_puts(CyStr* s, const char* p) { cy_str_putn(s, p, strlen(p)); }
void cy_str_putc(CyStr* s, char c) { cy_str_putn(s, &c, 1); }

void cy_str_putcp(CyStr* s, uint32_t cp)
{
    char b[4];
    cy_str_putn(s, b, cy_utf8_put(b, cp));
}

void cy_str_free(CyStr* s) { free(s->p); s->p = NULL; s->n = s->cap = 0; }

int cy_utf8_next(const char* s, size_t len, size_t* i, uint32_t* cp)
{
    const unsigned char* u = (const unsigned char*)s + *i;
    size_t rest = len - *i;
    uint32_t c;
    int n, k;
    if (rest == 0) return 0;
    if (u[0] < 0x80) { *cp = u[0]; *i += 1; return 1; }
    if ((u[0] & 0xE0) == 0xC0) { c = u[0] & 0x1F; n = 2; }
    else if ((u[0] & 0xF0) == 0xE0) { c = u[0] & 0x0F; n = 3; }
    else if ((u[0] & 0xF8) == 0xF0) { c = u[0] & 0x07; n = 4; }
    else return 0;
    if (rest < (size_t)n) return 0;
    for (k = 1; k < n; k++) {
        if ((u[k] & 0xC0) != 0x80) return 0;
        c = (c << 6) | (u[k] & 0x3F);
    }
    if ((n == 2 && c < 0x80) || (n == 3 && c < 0x800) || (n == 4 && c < 0x10000) ||
        c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF))
        return 0;
    *cp = c;
    *i += (size_t)n;
    return 1;
}

size_t cy_utf8_put(char* o, uint32_t c)
{
    if (c < 0x80) { o[0] = (char)c; return 1; }
    if (c < 0x800) { o[0] = (char)(0xC0 | (c >> 6)); o[1] = (char)(0x80 | (c & 0x3F)); return 2; }
    if (c < 0x10000) {
        o[0] = (char)(0xE0 | (c >> 12)); o[1] = (char)(0x80 | ((c >> 6) & 0x3F)); o[2] = (char)(0x80 | (c & 0x3F));
        return 3;
    }
    o[0] = (char)(0xF0 | (c >> 18)); o[1] = (char)(0x80 | ((c >> 12) & 0x3F));
    o[2] = (char)(0x80 | ((c >> 6) & 0x3F)); o[3] = (char)(0x80 | (c & 0x3F));
    return 4;
}

/* ---- 品詞 ---- */
static int field_eq(const char* pos, int field, const char* v)
{
    size_t n = strlen(v);
    while (field-- > 0) {
        pos = strchr(pos, ',');
        if (!pos) return 0;
        pos++;
    }
    return strncmp(pos, v, n) == 0 && (pos[n] == ',' || pos[n] == '\0');
}

int cy_pos_has(const char* pos, int field, const char* value) { return pos && field_eq(pos, field, value); }
int cy_pos_is(const char* pos, const char* major) { return pos && field_eq(pos, 0, major); }
int cy_pos_is2(const char* pos, const char* major, const char* sub)
{
    return pos && field_eq(pos, 0, major) && field_eq(pos, 1, sub);
}

/* ---- カナ ---- */
uint32_t cy_to_katakana(uint32_t c)
{
    if (c >= 0x3041 && c <= 0x3096) return c + 0x60;
    if (c == 0x309D || c == 0x309E) return c + 0x60;
    return c;
}

static int is_small(uint32_t c)
{
    switch (c) {
    case 0x30A1: case 0x30A3: case 0x30A5: case 0x30A7: case 0x30A9:  /* ァィゥェォ */
    case 0x30E3: case 0x30E5: case 0x30E7: case 0x30EE:                /* ャュョヮ */
        return 1;
    default:
        return 0;
    }
}

int cy_count_mora(const char* kana)
{
    size_t i = 0, len = strlen(kana);
    uint32_t c;
    int n = 0;
    while (cy_utf8_next(kana, len, &i, &c)) {
        c = cy_to_katakana(c);
        if ((c >= 0x30A1 && c <= 0x30FA && !is_small(c)) || c == 0x30FC) n++;
    }
    return n;
}
