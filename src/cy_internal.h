/* ColorfulYomi 内部定義  SPDX-License-Identifier: MIT */
#ifndef CY_INTERNAL_H
#define CY_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include "colorful_yomi.h"

/* ---- 辞書ファイルの構造（リトルエンディアン、docs/dictionary-format.md） ---- */
typedef struct { uint32_t str, first; uint16_t n, len; } CySurf;
typedef struct { uint16_t left, right; int16_t cost; uint16_t pos; uint32_t pron, acc, chain; } CyWord;
typedef struct { uint32_t lo, hi; uint16_t prim, pad; uint32_t mask; } CyRange;
typedef struct { uint8_t invoke, group, length, pad; uint32_t name; } CyCat;
typedef struct { uint16_t cat, left, right; int16_t cost; uint16_t pos, pad; } CyUnk;

typedef struct {
    char* surface;      /* 正規化した UTF-8 */
    uint32_t len;
    char* pron;         /* カタカナ */
    char acc[16];       /* "a/m" */
    uint16_t left, right;
    int16_t cost;
    const char* pos;
} CyUser;

enum { CY_CAT_MAX = 32 };

struct CyDic {
    uint8_t* owned;            /* cy_open で確保した領域（cy_open_memory では NULL） */
    const uint8_t* data;
    size_t size;
    const CySurf* surf;  uint32_t nsurf;
    const CyWord* word;  uint32_t nword;
    const int16_t* matrix; uint32_t lsize, rsize;
    const CyRange* range; uint32_t nrange;
    const CyCat* cat;    uint32_t ncat;
    const CyUnk* unk;    uint32_t nunk;
    const uint32_t* pos; uint32_t npos;
    const char* pool;    uint32_t npool;
    int cat_default, cat_space, cat_kanji, cat_hira, cat_kata, cat_alpha, cat_numeric;
    CyUser* user; size_t nuser, capuser;
};

/* ---- 文字列・メモリ ---- */
typedef struct CyBlock CyBlock;
typedef struct { CyBlock* head; int oom; } CyArena;
void* cy_alloc(CyArena* a, size_t n);
char* cy_strndup(CyArena* a, const char* s, size_t n);
char* cy_strdup(CyArena* a, const char* s);
void  cy_arena_free(CyArena* a);

typedef struct { char* p; size_t n, cap; int oom; } CyStr;
void cy_str_putn(CyStr* s, const char* p, size_t n);
void cy_str_puts(CyStr* s, const char* p);
void cy_str_putc(CyStr* s, char c);
void cy_str_putcp(CyStr* s, uint32_t cp);
void cy_str_free(CyStr* s);

/* UTF-8 を 1 文字読む。不正なら 0 */
int cy_utf8_next(const char* s, size_t len, size_t* i, uint32_t* cp);
size_t cy_utf8_put(char* out, uint32_t cp);

/* ---- 辞書の参照 ---- */
void cy_char_info(const CyDic* d, uint32_t cp, int* prim, uint32_t* mask);
/* key に一致する表層形の番号。なければ -1。*prefix_exists に key で始まる表層形があるかを返す */
long cy_find_surface(const CyDic* d, const char* key, size_t klen, int* prefix_exists);
long cy_find_user(const CyDic* d, const char* key, size_t klen, int* prefix_exists);
const char* cy_pool(const CyDic* d, uint32_t off);
int cy_conn_cost(const CyDic* d, int right, int left);

/* ---- 解析結果（形態素） ---- */
enum { MK_WORD = 0, MK_USER, MK_UNK, MK_NUMBER };
typedef struct {
    int kind;
    const char* surf; size_t slen;  /* 正規化した UTF-8 */
    const char* pos;                /* "品詞,細分類1,細分類2,細分類3,活用型,活用形" */
    const char* pron;               /* カタカナ。複数句の語は ':' で区切る。"’" は無声化の印 */
    const char* acc;                /* "a/m"（複数句なら ':' で区切る） */
    const char* chain;              /* アクセント結合規則 */
    int cat;                        /* 未知語の文字種 */
    const char* digits;             /* 数（MK_NUMBER）: ASCII の数字列 */
    int dlen;
} CyMor;

int cy_analyze(const CyDic* d, const char* text, CyArena* a, CyMor** out, size_t* n);
/* 正規化（英数字・記号を全角に、半角カナを全角に）。不正な UTF-8 なら CT_ERR_UTF8 */
int cy_normalize(const char* in, size_t len, CyStr* out);

/* 数字 */
/* digitwise: 1 けたずつ読む（電話番号など） */
const char* cy_number_reading(CyArena* a, const char* digits, int dlen, int digitwise, int* acc);
void cy_apply_counters(CyArena* a, CyMor* m, size_t n);

/* 句とアクセント → カナ記号列 */
int cy_build_koe(const CyMor* m, size_t n, CyStr* out);
/* カナ記号列 → ローマ字記号列 */
int cy_koe_to_roman(const char* koe, CyStr* out);

/* 品詞の判定 */
int cy_pos_is(const char* pos, const char* major);                 /* 品詞 */
int cy_pos_is2(const char* pos, const char* major, const char* sub); /* 品詞, 細分類1 */
int cy_pos_has(const char* pos, int field, const char* value);

/* カナ */
int  cy_count_mora(const char* kana);
uint32_t cy_to_katakana(uint32_t cp);

#endif
