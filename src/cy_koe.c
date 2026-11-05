/* ColorfulYomi: アクセント句・アクセント核を決めてカナ記号列を作る  SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cy_internal.h"

/* 1 つの句に入る語（複数句の語は分けたもの） */
typedef struct {
    const char* pron;   /* カタカナ（':' を含まない） */
    size_t plen;
    int acc, mora;
    const char* pos;
    const char* chain;
    int kind;
    int split;          /* 複数句の語の 2 つ目以降 */
} Unit;

enum { P_NONE = 0, P_SEP, P_COMMA, P_PERIOD, P_EXCL, P_QUEST };

static int punct_of(const CyMor* m)
{
    /* 読みのない未知の記号（－ ＃ など、名詞として解析されたもの）は句の区切り */
    if (m->kind == MK_UNK && (!m->pron || cy_count_mora(m->pron) == 0)) return P_SEP;
    if (!cy_pos_is(m->pos, "記号")) return -1;
    if (strcmp(m->surf, "？") == 0) return P_QUEST;
    if (strcmp(m->surf, "！") == 0) return P_EXCL;
    if (cy_pos_is2(m->pos, "記号", "句点") || strcmp(m->surf, "．") == 0) return P_PERIOD;
    if (cy_pos_is2(m->pos, "記号", "読点")) return P_COMMA;
    if (m->pron && cy_count_mora(m->pron) > 0 && !cy_pos_is2(m->pos, "記号", "空白")) return -1;  /* 読める記号（＆ など） */
    return P_SEP;
}

static int is_noun(const Unit* u) { return cy_pos_is(u->pos, "名詞") || u->kind == MK_NUMBER; }

static int is_letter(const Unit* u)
{
    return cy_pos_is2(u->pos, "記号", "アルファベット");
}

static int is_ancillary(const Unit* u)
{
    return cy_pos_is(u->pos, "助詞") || cy_pos_is(u->pos, "助動詞");
}

/* cur を前の句につなげるか */
static int attaches(const Unit* prev, const Unit* cur)
{
    if (!prev || cur->split) return 0;
    if (is_ancillary(cur)) return 1;
    if (cy_pos_is2(cur->pos, "名詞", "接尾") || cy_pos_is2(cur->pos, "名詞", "非自立") ||
        cy_pos_is2(cur->pos, "名詞", "特殊"))
        return 1;
    if (cy_pos_is2(cur->pos, "動詞", "接尾") || cy_pos_is2(cur->pos, "動詞", "非自立") ||
        cy_pos_is2(cur->pos, "形容詞", "接尾") || cy_pos_is2(cur->pos, "形容詞", "非自立"))
        return 1;
    if (cy_pos_is(prev->pos, "接頭詞")) return 1;
    if (is_letter(prev) && is_letter(cur)) return 1;   /* 英字の並び（AI, NHK） */
    /* 数 + 名詞（11 月、3 人、5 日 など） */
    if ((prev->kind == MK_NUMBER || cy_pos_is2(prev->pos, "名詞", "数")) && cy_pos_is(cur->pos, "名詞") &&
        cur->kind != MK_NUMBER && !cy_pos_is2(cur->pos, "名詞", "数"))
        return 1;
    if (cur->kind == MK_NUMBER || cy_pos_is2(cur->pos, "名詞", "数")) return cy_pos_is2(prev->pos, "名詞", "数") && prev->kind != MK_NUMBER;
    if (cy_pos_is(cur->pos, "名詞") && is_noun(prev) &&
        !cy_pos_is2(prev->pos, "名詞", "代名詞") && !cy_pos_is2(cur->pos, "名詞", "代名詞") &&
        !cy_pos_is2(prev->pos, "名詞", "副詞可能") && !cy_pos_is2(cur->pos, "名詞", "副詞可能") &&
        !cy_pos_is2(prev->pos, "名詞", "数") && prev->kind != MK_NUMBER)
        return 1;
    /* サ変名詞 + する */
    if (cy_pos_is(cur->pos, "動詞") && cy_pos_has(cur->pos, 4, "サ変・スル") && cy_pos_is2(prev->pos, "名詞", "サ変接続"))
        return 1;
    return 0;
}

static const char* major_name(const Unit* u)
{
    static const char* NAMES[] = { "名詞", "動詞", "形容詞", "助詞", "助動詞", "副詞", "連体詞", "接頭詞" };
    size_t i;
    if (u->kind == MK_NUMBER) return "名詞";
    for (i = 0; i < sizeof NAMES / sizeof NAMES[0]; i++)
        if (cy_pos_is(u->pos, NAMES[i])) return NAMES[i];
    return "";
}

/* "名詞%F2@1/動詞%F1" から前の語の品詞に合う規則を探す */
static int find_frule(const char* chain, const char* prevmajor, int* type, int* add)
{
    const char* p = chain;
    size_t ml = strlen(prevmajor);
    if (!*prevmajor) return 0;
    while (p && *p) {
        const char* end = strchr(p, '/');
        size_t len = end ? (size_t)(end - p) : strlen(p);
        if (len > ml + 2 && strncmp(p, prevmajor, ml) == 0 && p[ml] == '%' && p[ml + 1] == 'F') {
            *type = atoi(p + ml + 2);
            *add = 0;
            {
                const char* at = memchr(p, '@', len);
                if (at) *add = atoi(at + 1);
            }
            return 1;
        }
        p = end ? end + 1 : NULL;
    }
    return 0;
}

/* 句の中で次の語をつないだときのアクセント核 */
static int combine(int acc, int mora, const Unit* prev, const Unit* w)
{
    int type, add;
    const char* r = w->chain ? w->chain : "";
    /* 接頭詞の規則（P1: 後ろの語に一体化、P2: 後ろの語の核を残す） */
    if (cy_pos_is(prev->pos, "接頭詞") && prev->chain && prev->chain[0] == 'P') {
        if (prev->chain[1] == '1') return w->acc ? mora + w->acc : 0;
        return mora + (w->acc ? w->acc : 1);
    }
    if (is_letter(prev) && is_letter(w)) return mora + 1;   /* 最後の文字の 1 モーラ目 */
    if (r[0] == 'C' && r[1] >= '1' && r[1] <= '5' && (is_noun(prev) || cy_pos_is(prev->pos, "接頭詞"))) {
        switch (r[1]) {
        case '1': return w->acc ? mora + w->acc : 0;   /* 後ろの語の核を残す */
        case '2': return mora + 1;                       /* 後ろの語の 1 モーラ目 */
        case '3': return mora;                           /* 前の語の最後のモーラ */
        case '4': return 0;                              /* 平板 */
        default:  return acc;                            /* 前の語の核を残す */
        }
    }
    if (find_frule(r, major_name(prev), &type, &add)) {
        switch (type) {
        case 2: return acc == 0 ? mora + add : acc;     /* 前が平板なら付属語に核 */
        case 3: return acc != 0 ? mora + add : acc;     /* 前に核があれば付属語へ移す */
        case 4: return mora + add;                       /* 付属語に核 */
        case 5: return 0;                                /* 平板にする */
        default: return acc;                             /* 前の語に従う */
        }
    }
    if (is_noun(w) && !is_ancillary(w) && is_noun(prev)) {
        /* 規則のない名詞の複合: 後ろの語の核を残し、平板なら 1 モーラ目 */
        if (cy_pos_is2(w->pos, "名詞", "接尾") || cy_pos_is2(w->pos, "名詞", "非自立")) return acc;
        return w->acc ? mora + w->acc : mora + 1;
    }
    return acc;
}

/* 特殊モーラ（ー ッ ン、小さい母音字だけのもの）か */
static int special_mora_at(const char* kana, int k)
{
    size_t i = 0, len = strlen(kana);
    uint32_t c;
    int n = 0;
    while (cy_utf8_next(kana, len, &i, &c)) {
        int small = c == 0x30A1 || c == 0x30A3 || c == 0x30A5 || c == 0x30A7 || c == 0x30A9 ||
                    c == 0x30E3 || c == 0x30E5 || c == 0x30E7 || c == 0x30EE;
        if (small) continue;
        if (c == 0x2019) continue;
        n++;
        if (n == k) return c == 0x30FC || c == 0x30C3 || c == 0x30F3;
    }
    return 0;
}

/* カタカナの発音をひらがなで出し、k モーラ目の後に ' を入れる */
static void put_kana(CyStr* o, const char* kana, size_t len, int acc)
{
    size_t i = 0;
    uint32_t c;
    int n = 0, placed = acc <= 0;
    while (i < len) {
        size_t save = i;
        int small;
        if (!cy_utf8_next(kana, len, &i, &c)) break;
        if (c == 0x2019 || c == '\'' ) continue;               /* 無声化の印は合成側に任せる */
        small = c == 0x30A1 || c == 0x30A3 || c == 0x30A5 || c == 0x30A7 || c == 0x30A9 ||
                c == 0x30E3 || c == 0x30E5 || c == 0x30E7 || c == 0x30EE;
        if (!small && !placed && n == acc) { cy_str_putc(o, '\''); placed = 1; }
        (void)save;
        if (!small) n++;
        if (c == 0x30F5) c = 0x30AB;           /* ヵ → カ */
        else if (c == 0x30F6) c = 0x30B1;      /* ヶ → ケ */
        else if (c == 0x30EE) c = 0x30EF;      /* ヮ → ワ */
        else if (c == 0x30F0) c = 0x30A4;      /* ヰ → イ */
        else if (c == 0x30F1) c = 0x30A8;      /* ヱ → エ */
        else if (c == 0x30F7) c = 0x30F4;      /* ヷ → ヴ */
        if (c >= 0x30A1 && c <= 0x30F6) c -= 0x60;   /* ひらがなへ */
        if ((c >= 0x3041 && c <= 0x3094) || c == 0x30FC) cy_str_putcp(o, c);
    }
    if (!placed && n == acc) cy_str_putc(o, '\'');
}

/* 句を書き出す */
static void emit_phrase(CyStr* o, const Unit* us, size_t n, int acc, int mora, int* pending, int* have)
{
    CyStr kana = { 0 };
    size_t i;
    if (mora == 0) return;
    for (i = 0; i < n; i++) cy_str_putn(&kana, us[i].pron, us[i].plen);
    if (kana.oom) { o->oom = 1; cy_str_free(&kana); return; }
    if (acc > mora) acc = mora;
    while (acc > 1 && special_mora_at(kana.p, acc)) acc--;
    if (acc == 1 && special_mora_at(kana.p, 1)) acc = 0;
    if (*have) {
        static const char* SEP2[] = { "/", "/", "、", "。", "！", "？" };
        cy_str_puts(o, SEP2[*pending]);
    }
    put_kana(o, kana.p, kana.n, acc);
    *have = 1;
    *pending = P_SEP;
    cy_str_free(&kana);
}

/* 句頭に置けない（ー・ッ で始まる）なら前の句につなぐ */
static int bad_head(const Unit* u)
{
    return u->plen >= 3 && (memcmp(u->pron, "ー", 3) == 0 || memcmp(u->pron, "ッ", 3) == 0);
}

int cy_build_koe(const CyMor* m, size_t n, CyStr* o)
{
    static const char* SEP[] = { "/", "/", "、", "。", "！", "？" };
    Unit* us = (Unit*)malloc((n * 8 + 1) * sizeof *us);
    size_t nu = 0, i, start = 0;
    int pending = P_NONE, have = 0, acc = 0, mora = 0;
    if (!us) return CT_ERR_MEMORY;

    for (i = 0; i <= n; i++) {
        int pu = i < n ? punct_of(&m[i]) : P_PERIOD + 100;
        if (pu == P_SEP && i > 0 && i + 1 < n && m[i - 1].kind == MK_NUMBER && m[i + 1].kind == MK_NUMBER)
            pu = P_COMMA;   /* 090-1234 の「-」は短い間 */
        Unit cur[8];
        int ncur = 0, k;
        if (i < n && pu < 0) {
            /* 語を単位に分ける（"アリガトー:ゴザイマス" のような複数句の語） */
            const char* p = m[i].pron ? m[i].pron : "";
            const char* a = m[i].acc ? m[i].acc : "0/0";
            int split = 0;
            while (ncur < 8) {
                const char* pe = strchr(p, ':');
                const char* ae = strchr(a, ':');
                Unit* u = &cur[ncur];
                u->pron = p;
                u->plen = pe ? (size_t)(pe - p) : strlen(p);
                u->acc = atoi(a);
                u->mora = 0;
                u->pos = m[i].pos; u->chain = m[i].chain; u->kind = m[i].kind;
                u->split = split;
                {
                    char tmp[256];
                    size_t l = u->plen < sizeof tmp - 1 ? u->plen : sizeof tmp - 1;
                    memcpy(tmp, p, l); tmp[l] = '\0';
                    u->mora = cy_count_mora(tmp);
                }
                if (u->acc > u->mora) u->acc = u->mora;
                if (u->acc < 0) u->acc = 0;
                ncur++;
                split = 1;
                if (!pe) break;
                p = pe + 1;
                a = ae ? ae + 1 : "0/0";
            }
        }
        if (i == n || pu >= 0) {
            /* 句を閉じる */
            if (nu > start) emit_phrase(o, us + start, nu - start, acc, mora, &pending, &have);
            start = nu;
            acc = mora = 0;
            if (i < n) {
                if (have && pu > pending) pending = pu;
            } else if (have && pending >= P_COMMA) {
                cy_str_puts(o, SEP[pending]);
            }
            continue;
        }
        for (k = 0; k < ncur; k++) {
            Unit* u = &cur[k];
            const Unit* prev = nu > start ? &us[nu - 1] : NULL;
            if (u->mora == 0) continue;   /* 読みのない語（読めない漢字・消えた助数詞） */
            while (!prev && bad_head(u)) {   /* 句頭の ー ッ は落とす */
                u->pron += 3; u->plen -= 3; u->mora--;
                if (u->acc > u->mora) u->acc = u->mora;
            }
            if (u->mora == 0) continue;
            if (prev && (attaches(prev, u) || bad_head(u))) {
                acc = combine(acc, mora, prev, u);
                mora += u->mora;
                if (acc > mora) acc = mora;
                if (acc < 0) acc = 0;
            } else {
                if (nu > start) emit_phrase(o, us + start, nu - start, acc, mora, &pending, &have);
                start = nu;
                acc = u->acc; mora = u->mora;
            }
            us[nu++] = *u;
        }
    }
    free(us);
    return o->oom ? CT_ERR_MEMORY : CT_OK;
}
