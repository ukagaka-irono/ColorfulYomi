/* ColorfulYomi: 辞書の読み込みと検索  SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cy_internal.h"

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#define HEADER_SIZE 64

static uint32_t rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

const char* cy_version(void) { return CY_VERSION; }

static FILE* u8fopen(const char* path, const char* mode)
{
#ifdef _WIN32
    wchar_t wp[1024], wm[8];
    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wp, 1024)) return NULL;
    if (!MultiByteToWideChar(CP_UTF8, 0, mode, -1, wm, 8)) return NULL;
    return _wfopen(wp, wm);
#else
    return fopen(path, mode);
#endif
}

static int find_cat(CyDic* d, const char* name)
{
    uint32_t i;
    for (i = 0; i < d->ncat; i++)
        if (strcmp(cy_pool(d, d->cat[i].name), name) == 0) return (int)i;
    return -1;
}

static int setup(CyDic* d)
{
    const uint8_t* p = d->data;
    size_t off = HEADER_SIZE, need;
    uint32_t i;
    const uint16_t one = 1;
    if (*(const uint8_t*)&one != 1) return CT_ERR_DIC;          /* ビッグエンディアンは非対応 */
    if (d->size < HEADER_SIZE || memcmp(p, "CTDIC\0\0\0", 8) != 0 || rd32(p + 8) != 1) return CT_ERR_DIC;
    if (((uintptr_t)p & 3) != 0) return CT_ERR_DIC;
    d->nsurf = rd32(p + 12); d->nword = rd32(p + 16);
    d->lsize = rd16(p + 20); d->rsize = rd16(p + 22);
    d->nrange = rd32(p + 24); d->ncat = rd32(p + 28); d->nunk = rd32(p + 32);
    d->npos = rd32(p + 36); d->npool = rd32(p + 40);
#define SECTION(ptr, type, count) do { \
        need = (size_t)(count) * sizeof(type); \
        if (need / sizeof(type) != (size_t)(count) || off + need > d->size) return CT_ERR_DIC; \
        ptr = (const type*)(p + off); off += (need + 3) & ~(size_t)3; } while (0)
    SECTION(d->surf, CySurf, d->nsurf);
    SECTION(d->word, CyWord, d->nword);
    SECTION(d->matrix, int16_t, (size_t)d->lsize * d->rsize);
    SECTION(d->range, CyRange, d->nrange);
    SECTION(d->cat, CyCat, d->ncat);
    SECTION(d->unk, CyUnk, d->nunk);
    SECTION(d->pos, uint32_t, d->npos);
    SECTION(d->pool, char, d->npool);
#undef SECTION
    if (d->npool == 0 || d->pool[d->npool - 1] != '\0' || d->ncat == 0 || d->ncat > CY_CAT_MAX) return CT_ERR_DIC;
    /* 参照の範囲を確かめる */
    for (i = 0; i < d->nsurf; i++) {
        const CySurf* s = &d->surf[i];
        if (s->str + (uint32_t)s->len >= d->npool || s->first + s->n > d->nword || s->n == 0) return CT_ERR_DIC;
    }
    for (i = 0; i < d->nword; i++) {
        const CyWord* w = &d->word[i];
        if (w->pron >= d->npool || w->acc >= d->npool || w->chain >= d->npool || w->pos >= d->npos ||
            w->left >= d->rsize || w->right >= d->lsize)
            return CT_ERR_DIC;
    }
    for (i = 0; i < d->npos; i++) if (d->pos[i] >= d->npool) return CT_ERR_DIC;
    for (i = 0; i < d->ncat; i++) if (d->cat[i].name >= d->npool) return CT_ERR_DIC;
    for (i = 0; i < d->nrange; i++) if (d->range[i].prim >= d->ncat) return CT_ERR_DIC;
    for (i = 0; i < d->nunk; i++) {
        const CyUnk* u = &d->unk[i];
        if (u->cat >= d->ncat || u->pos >= d->npos || u->left >= d->rsize || u->right >= d->lsize) return CT_ERR_DIC;
    }
    d->cat_default = find_cat(d, "DEFAULT");
    d->cat_space = find_cat(d, "SPACE");
    d->cat_kanji = find_cat(d, "KANJI");
    d->cat_hira = find_cat(d, "HIRAGANA");
    d->cat_kata = find_cat(d, "KATAKANA");
    d->cat_alpha = find_cat(d, "ALPHA");
    d->cat_numeric = find_cat(d, "NUMERIC");
    if (d->cat_default < 0) return CT_ERR_DIC;
    return CT_OK;
}

int cy_open_memory(const void* data, size_t size, CyDic** out)
{
    CyDic* d;
    int r;
    if (!data || !out) return CT_ERR_NULL;
    *out = NULL;
    d = (CyDic*)calloc(1, sizeof *d);
    if (!d) return CT_ERR_MEMORY;
    d->data = (const uint8_t*)data;
    d->size = size;
    r = setup(d);
    if (r != CT_OK) { free(d); return r; }
    *out = d;
    return CT_OK;
}

int cy_open(const char* path, CyDic** out)
{
    FILE* f;
    long len;
    uint8_t* buf;
    int r;
    if (!path || !out) return CT_ERR_NULL;
    *out = NULL;
    f = u8fopen(path, "rb");
    if (!f) return CT_ERR_IO;
    if (fseek(f, 0, SEEK_END) != 0 || (len = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return CT_ERR_IO; }
    buf = (uint8_t*)malloc((size_t)len + 1);
    if (!buf) { fclose(f); return CT_ERR_MEMORY; }
    if (fread(buf, 1, (size_t)len, f) != (size_t)len) { fclose(f); free(buf); return CT_ERR_IO; }
    fclose(f);
    r = cy_open_memory(buf, (size_t)len, out);
    if (r != CT_OK) { free(buf); return r; }
    (*out)->owned = buf;
    return CT_OK;
}

void cy_close(CyDic* d)
{
    size_t i;
    if (!d) return;
    for (i = 0; i < d->nuser; i++) { free(d->user[i].surface); free(d->user[i].pron); }
    free(d->user);
    free(d->owned);
    free(d);
}

const char* cy_pool(const CyDic* d, uint32_t off) { return d->pool + off; }

int cy_conn_cost(const CyDic* d, int right, int left) { return d->matrix[(size_t)right * d->rsize + (size_t)left]; }

void cy_char_info(const CyDic* d, uint32_t cp, int* prim, uint32_t* mask)
{
    uint32_t lo = 0, hi = d->nrange;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        if (d->range[mid].hi < cp) lo = mid + 1;
        else hi = mid;
    }
    if (lo < d->nrange && d->range[lo].lo <= cp && cp <= d->range[lo].hi) {
        *prim = d->range[lo].prim;
        *mask = d->range[lo].mask;
    } else {
        *prim = d->cat_default;
        *mask = 1u << d->cat_default;
    }
}

static int cmp_bytes(const char* a, size_t alen, const char* b, size_t blen)
{
    size_t n = alen < blen ? alen : blen;
    int c = memcmp(a, b, n);
    if (c) return c;
    return alen < blen ? -1 : alen > blen ? 1 : 0;
}

long cy_find_surface(const CyDic* d, const char* key, size_t klen, int* prefix_exists)
{
    uint32_t lo = 0, hi = d->nsurf;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        const CySurf* s = &d->surf[mid];
        if (cmp_bytes(d->pool + s->str, s->len, key, klen) < 0) lo = mid + 1;
        else hi = mid;
    }
    *prefix_exists = 0;
    if (lo < d->nsurf) {
        const CySurf* s = &d->surf[lo];
        if (s->len >= klen && memcmp(d->pool + s->str, key, klen) == 0) {
            *prefix_exists = 1;
            if (s->len == klen) return (long)lo;
        }
    }
    return -1;
}

long cy_find_user(const CyDic* d, const char* key, size_t klen, int* prefix_exists)
{
    size_t lo = 0, hi = d->nuser;
    *prefix_exists = 0;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (cmp_bytes(d->user[mid].surface, d->user[mid].len, key, klen) < 0) lo = mid + 1;
        else hi = mid;
    }
    if (lo < d->nuser && d->user[lo].len >= klen && memcmp(d->user[lo].surface, key, klen) == 0) {
        *prefix_exists = 1;
        if (d->user[lo].len == klen) return (long)lo;
    }
    return -1;
}

/* ---- ユーザー辞書 ---- */
static const struct { const char* name; const char* pos; } USER_POS[] = {
    { "名詞", "名詞,一般" },
    { "普通名詞", "名詞,一般" },
    { "サ変名詞", "名詞,サ変接続" },
    { "固有名詞", "名詞,固有名詞,一般" },
    { "人名", "名詞,固有名詞,人名,一般" },
    { "地名", "名詞,固有名詞,地域,一般" },
    { "組織", "名詞,固有名詞,組織" },
    { "感動詞", "感動詞" },
};

static int user_cmp(const void* a, const void* b)
{
    const CyUser* x = (const CyUser*)a;
    const CyUser* y = (const CyUser*)b;
    return cmp_bytes(x->surface, x->len, y->surface, y->len);
}

int cy_add_user_word(CyDic* d, const char* surface, const char* reading, int accent, const char* posname)
{
    CyStr ns = { 0 }, nr = { 0 };
    const char* posstr = NULL;
    const CyUnk* u = NULL;
    size_t i, len;
    uint32_t c;
    int mora, r;
    CyUser w;
    if (!d || !surface || !reading) return CT_ERR_NULL;
    if (!posname || !*posname) posname = "名詞";
    for (i = 0; i < sizeof USER_POS / sizeof USER_POS[0]; i++)
        if (strcmp(USER_POS[i].name, posname) == 0) { posstr = USER_POS[i].pos; break; }
    if (!posstr) return CT_ERR_PARAM;
    for (i = 0; i < d->nunk && !u; i++) {
        const char* p = cy_pool(d, d->pos[d->unk[i].pos]);
        size_t n = strlen(posstr);
        if (strncmp(p, posstr, n) == 0 && (p[n] == ',' || p[n] == '\0')) u = &d->unk[i];
    }
    if (!u) return CT_ERR_PARAM;
    r = cy_normalize(surface, strlen(surface), &ns);
    if (r != CT_OK || ns.n == 0) { cy_str_free(&ns); return r != CT_OK ? r : CT_ERR_PARAM; }
    /* 読み: ひらがな・カタカナ・長音のみ */
    len = strlen(reading);
    i = 0;
    while (i < len) {
        if (!cy_utf8_next(reading, len, &i, &c)) { cy_str_free(&ns); cy_str_free(&nr); return CT_ERR_UTF8; }
        c = cy_to_katakana(c);
        if (!((c >= 0x30A1 && c <= 0x30FA) || c == 0x30FC)) { cy_str_free(&ns); cy_str_free(&nr); return CT_ERR_CHAR; }
        cy_str_putcp(&nr, c);
    }
    mora = nr.n ? cy_count_mora(nr.p) : 0;
    if (mora == 0 || accent < 0 || accent > mora) { cy_str_free(&ns); cy_str_free(&nr); return CT_ERR_PARAM; }
    if (ns.oom || nr.oom) { cy_str_free(&ns); cy_str_free(&nr); return CT_ERR_MEMORY; }
    memset(&w, 0, sizeof w);
    w.surface = ns.p; w.len = (uint32_t)ns.n;
    w.pron = nr.p;
    snprintf(w.acc, sizeof w.acc, "%d/%d", accent, mora);
    w.left = u->left; w.right = u->right;
    w.cost = -10000;               /* 辞書の語より優先する */
    w.pos = cy_pool(d, d->pos[u->pos]);
    /* 同じ表層形があれば置き換える */
    for (i = 0; i < d->nuser; i++) {
        if (d->user[i].len == w.len && memcmp(d->user[i].surface, w.surface, w.len) == 0) {
            free(d->user[i].surface); free(d->user[i].pron);
            d->user[i] = w;
            return CT_OK;
        }
    }
    if (d->nuser == d->capuser) {
        size_t cap = d->capuser ? d->capuser * 2 : 16;
        CyUser* nu = (CyUser*)realloc(d->user, cap * sizeof *nu);
        if (!nu) { free(w.surface); free(w.pron); return CT_ERR_MEMORY; }
        d->user = nu; d->capuser = cap;
    }
    d->user[d->nuser++] = w;
    qsort(d->user, d->nuser, sizeof *d->user, user_cmp);
    return CT_OK;
}

/* CSV の 1 列を切り出す（引用符なし） */
static char* next_field(char** p)
{
    char* s = *p;
    char* e;
    if (!s) return NULL;
    e = strchr(s, ',');
    if (e) { *e = '\0'; *p = e + 1; } else *p = NULL;
    while (*s == ' ' || *s == '\t') s++;
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = '\0';
    return s;
}

int cy_add_user_dic(CyDic* d, const char* path, int* error_line)
{
    FILE* f;
    char line[1024];
    int ln = 0;
    if (error_line) *error_line = 0;
    if (!d || !path) return CT_ERR_NULL;
    f = u8fopen(path, "rb");
    if (!f) return CT_ERR_IO;
    while (fgets(line, sizeof line, f)) {
        char* p = line;
        char *surf, *read, *acc, *pos;
        int r;
        ln++;
        if (ln == 1 && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) p += 3;
        if (p[0] == '#' || p[0] == '\r' || p[0] == '\n' || p[0] == '\0') continue;
        surf = next_field(&p); read = next_field(&p); acc = next_field(&p); pos = next_field(&p);
        if (ln == 1 && surf && strcmp(surf, "表層形") == 0) continue;    /* 見出し行 */
        if (!surf || !read || !acc || !*surf || !*read || !*acc) {
            fclose(f);
            if (error_line) *error_line = ln;
            return CT_ERR_PARAM;
        }
        r = cy_add_user_word(d, surf, read, atoi(acc), pos);
        if (r != CT_OK) {
            fclose(f);
            if (error_line) *error_line = ln;
            return r;
        }
    }
    fclose(f);
    return CT_OK;
}
