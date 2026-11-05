/* ColorfulYomi: 数の読みと助数詞による音の変化  SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include "cy_internal.h"

static const char* ONES[10] = { "", "イチ", "ニ", "サン", "ヨン", "ゴ", "ロク", "ナナ", "ハチ", "キュー" };
static const int ONES_ACC[10] = { 0, 2, 1, 1, 1, 1, 2, 1, 2, 1 };   /* 複合した数の末尾にあるとき */
static const char* DIGIT1[10] = { "ゼロ", "イチ", "ニー", "サン", "ヨン", "ゴー", "ロク", "ナナ", "ハチ", "キュー" };
static const char* HUNDREDS[10] = { "", "ヒャク", "ニヒャク", "サンビャク", "ヨンヒャク", "ゴヒャク", "ロッピャク",
                                    "ナナヒャク", "ハッピャク", "キューヒャク" };
static const char* THOUSANDS[10] = { "", "セン", "ニセン", "サンゼン", "ヨンセン", "ゴセン", "ロクセン",
                                     "ナナセン", "ハッセン", "キューセン" };
static const char* UNITS[5] = { "", "マン", "オク", "チョー", "ケー" };

/* 末尾の語の前までのモーラ数 + 末尾の語の中の核 で数全体のアクセントを決める */
static void put(CyStr* s, const char* w, int* lastpos, int* lastacc, int acc)
{
    *lastpos = s->n ? cy_count_mora(s->p) : 0;
    *lastacc = acc;
    cy_str_puts(s, w);
}

const char* cy_number_reading(CyArena* a, const char* dg, int len, int digitwise, int* acc)
{
    CyStr s = { 0 };
    const char* dot = memchr(dg, '.', (size_t)len);
    int ilen = dot ? (int)(dot - dg) : len;
    int lastpos = 0, lastacc = 0, i;
    const char* r;

    if (ilen == 0) {
        cy_str_puts(&s, "ゼロ"); lastacc = 1;
    } else if (digitwise || (dg[0] == '0' && ilen > 1) || ilen > 20) {
        /* 0 で始まる番号や長すぎる数は 1 けたずつ */
        for (i = 0; i < ilen; i++) cy_str_puts(&s, DIGIT1[dg[i] - '0']);
        lastpos = 0; lastacc = 0;
    } else if (ilen == 1 && dg[0] == '0') {
        cy_str_puts(&s, "ゼロ"); lastacc = 1;
    } else {
        int ngroup = (ilen + 3) / 4, g;
        for (g = 0; g < ngroup; g++) {
            int unit = ngroup - 1 - g;
            int start = ilen - (unit + 1) * 4;
            int d4[4], k, v = 0;
            for (k = 0; k < 4; k++) {
                int p = start + k;
                d4[k] = (p >= 0 && p < ilen) ? dg[p] - '0' : 0;
                v = v * 10 + d4[k];
            }
            if (v == 0) continue;
            if (unit > 4) unit = 4;
            if (d4[0]) {
                if (d4[0] == 1 && unit > 0 && v == 1000) put(&s, "イッセン", &lastpos, &lastacc, 2);   /* 一千万 */
                else put(&s, THOUSANDS[d4[0]], &lastpos, &lastacc, d4[0] == 1 ? 1 : cy_count_mora(THOUSANDS[d4[0]]) - 1);
            }
            if (d4[1]) put(&s, HUNDREDS[d4[1]], &lastpos, &lastacc, d4[1] == 1 ? 2 : cy_count_mora(HUNDREDS[d4[1]]) - 1);
            if (d4[2]) {
                if (d4[2] == 1) put(&s, "ジュー", &lastpos, &lastacc, 1);
                else {
                    char w[32];
                    snprintf(w, sizeof w, "%sジュー", ONES[d4[2]]);
                    put(&s, w, &lastpos, &lastacc, cy_count_mora(ONES[d4[2]]) + 1);
                }
            }
            if (d4[3]) put(&s, ONES[d4[3]], &lastpos, &lastacc, ONES_ACC[d4[3]]);
            if (unit > 0) put(&s, UNITS[unit], &lastpos, &lastacc, 1);
        }
        /* 単独の 3 は平板（さん） */
        if (ilen == 1 && dg[0] == '3') lastacc = 0;
    }
    if (dot) {
        int p0 = s.n ? cy_count_mora(s.p) : 0;
        cy_str_puts(&s, "テン");
        for (i = ilen + 1; i < len; i++) cy_str_puts(&s, DIGIT1[dg[i] - '0']);
        *acc = p0 + 1;   /* 「て'ん」で下がる */
    } else {
        *acc = lastacc ? lastpos + lastacc : 0;
    }
    r = s.n ? cy_strndup(a, s.p, s.n) : "";
    cy_str_free(&s);
    return r;
}

/* ---- 助数詞 ---- */
enum {
    K_NONE = 0,
    K_HB,     /* 本・匹・杯: 1/6/8/10/100 → ッ+パ行、3/千/何 → バ行 */
    K_HP,     /* 分・泊・発…: 1/6/8/10/100 → ッ+パ行、3/4/千 → パ行 */
    K_K,      /* 個・回・階…: 1/6/8/10/100 → ッ */
    K_S,      /* 冊・歳…: 1/8/10 → ッ */
    K_T,      /* 頭・点…: 1/8/10 → ッ */
    K_GATSU,  /* 月（何月）: 4 → シ、7 → シチ、9 → ク */
    K_JI,     /* 時: 4 → ヨ、7 → シチ、9 → ク */
    K_YO,     /* 年・円・時間: 4 → ヨ */
    K_NIN,    /* 人: 1 → ひとり、2 → ふたり、4 → よにん */
    K_NICHI,  /* 日（日付・日数） */
    K_TSU     /* つ: ひとつ〜ここのつ */
};

typedef struct { const char* surf; const char* pron; int kind; int k3voiced; } Counter;

static const Counter COUNTERS[] = {
    { "本", "ホン", K_HB, 0 }, { "匹", "ヒキ", K_HB, 0 }, { "杯", "ハイ", K_HB, 0 },
    { "分", "フン", K_HP, 0 }, { "泊", "ハク", K_HP, 0 }, { "発", "ハツ", K_HP, 0 }, { "歩", "ホ", K_HP, 0 },
    { "敗", "ハイ", K_HP, 0 }, { "票", "ヒョー", K_HP, 0 }, { "品", "ヒン", K_HP, 0 }, { "編", "ヘン", K_HP, 0 },
    { "個", "コ", K_K, 0 }, { "回", "カイ", K_K, 0 }, { "階", "カイ", K_K, 1 }, { "件", "ケン", K_K, 0 },
    { "軒", "ケン", K_K, 1 }, { "曲", "キョク", K_K, 0 }, { "巻", "カン", K_K, 0 }, { "缶", "カン", K_K, 0 },
    { "か所", "カショ", K_K, 0 }, { "ヶ所", "カショ", K_K, 0 }, { "箇所", "カショ", K_K, 0 },
    { "か月", "カゲツ", K_K, 0 }, { "ヶ月", "カゲツ", K_K, 0 }, { "カ月", "カゲツ", K_K, 0 }, { "ヵ月", "カゲツ", K_K, 0 },
    { "か国", "カコク", K_K, 0 }, { "ヶ国", "カコク", K_K, 0 }, { "カ国", "カコク", K_K, 0 },
    { "校", "コー", K_K, 0 }, { "隻", "セキ", K_S, 0 },
    { "冊", "サツ", K_S, 0 }, { "歳", "サイ", K_S, 0 }, { "才", "サイ", K_S, 0 }, { "足", "ソク", K_S, 1 },
    { "週", "シュー", K_S, 0 }, { "週間", "シューカン", K_S, 0 }, { "勝", "ショー", K_S, 0 }, { "セット", "セット", K_S, 0 },
    { "頭", "トー", K_T, 0 }, { "点", "テン", K_T, 0 }, { "通", "ツー", K_T, 0 }, { "着", "チャク", K_T, 0 },
    { "丁目", "チョーメ", K_T, 0 }, { "トン", "トン", K_T, 0 },
    { "月", "ガツ", K_GATSU, 0 },
    { "時", "ジ", K_JI, 0 }, { "時間", "ジカン", K_JI, 0 },
    { "年", "ネン", K_YO, 0 }, { "円", "エン", K_YO, 0 }, { "年間", "ネンカン", K_YO, 0 }, { "年生", "ネンセー", K_YO, 0 },
    { "人", "ニン", K_NIN, 0 },
    { "日", "ニチ", K_NICHI, 0 }, { "日間", "ニチカン", K_NICHI, 0 },
    { "つ", "ツ", K_TSU, 0 },
};

static int ends_with(const char* s, const char* t)
{
    size_t a = strlen(s), b = strlen(t);
    return a >= b && strcmp(s + a - b, t) == 0;
}

/* 先頭のカナをパ行・バ行に変える（ハ行の語のみ） */
static void shift_row(CyStr* o, const char* pron, int to_p)
{
    size_t i = 0, len = strlen(pron);
    uint32_t c;
    if (!cy_utf8_next(pron, len, &i, &c)) return;
    if (c >= 0x30CF && c <= 0x30DB && (c - 0x30CF) % 3 == 0) c += to_p ? 2 : 1;
    cy_str_putcp(o, c);
    cy_str_puts(o, pron + i);
}

/* 末尾の語を置き換える */
static const char* replace_tail(CyArena* a, const char* s, const char* from, const char* to)
{
    size_t n = strlen(s) - strlen(from);
    CyStr o = { 0 };
    const char* r;
    cy_str_putn(&o, s, n);
    cy_str_puts(&o, to);
    r = cy_strndup(a, o.p, o.n);
    cy_str_free(&o);
    return r;
}

/* 数の末尾の「イチ・ロク・ハチ・ジュー・ヒャク」を促音にする */
static const char* geminate(CyArena* a, const char* num, int with_100)
{
    if (ends_with(num, "イチ")) return replace_tail(a, num, "イチ", "イッ");
    if (ends_with(num, "ロク")) return replace_tail(a, num, "ロク", "ロッ");
    if (ends_with(num, "ハチ")) return replace_tail(a, num, "ハチ", "ハッ");
    if (ends_with(num, "ジュー")) return replace_tail(a, num, "ジュー", "ジュッ");
    if (with_100 && (ends_with(num, "ャク"))) return replace_tail(a, num, "ク", "ッ");
    return NULL;
}

/* 数の値（読みの末尾から求める簡易版。数字の並びがあればそちらを使う） */
static long num_value(const CyMor* m)
{
    static const char* KANJI[] = { "〇", "一", "二", "三", "四", "五", "六", "七", "八", "九", "十" };
    long v = 0;
    int i, k;
    if (m->kind == MK_NUMBER) {
        if (memchr(m->digits, '.', (size_t)m->dlen) || m->dlen > 9) return -1;
        for (i = 0; i < m->dlen; i++) v = v * 10 + (m->digits[i] - '0');
        return v;
    }
    for (k = 0; k <= 10; k++) if (strcmp(m->surf, KANJI[k]) == 0) return k;
    return -1;
}

static void set_mor(CyArena* a, CyMor* m, const char* pron, int acc)
{
    char buf[16];
    m->pron = pron;
    snprintf(buf, sizeof buf, "%d/%d", acc, cy_count_mora(pron));
    m->acc = cy_strdup(a, buf);
}

static int is_number(const CyMor* m)
{
    return m->kind == MK_NUMBER || cy_pos_is2(m->pos, "名詞", "数");
}

void cy_apply_counters(CyArena* a, CyMor* m, size_t n)
{
    size_t i;
    size_t k;
    for (i = 0; i + 1 < n; i++) {
        CyMor* num = &m[i];
        CyMor* ctr = &m[i + 1];
        const Counter* c = NULL;
        const char* np = num->pron;
        const char* g;
        CyStr o = { 0 };
        long v;
        int last;
        if (!is_number(num) || !np || !*np) continue;
        if (i + 2 < n && is_number(&m[i + 1])) continue;   /* 漢数字の途中 */
        for (k = 0; k < sizeof COUNTERS / sizeof COUNTERS[0]; k++)
            if (strcmp(ctr->surf, COUNTERS[k].surf) == 0) { c = &COUNTERS[k]; break; }
        if (!c) continue;
        if (!cy_pos_is(ctr->pos, "名詞")) continue;
        v = num_value(num);
        last = v >= 0 ? (int)(v % 10) : -1;
        ctr->pron = c->pron;
        switch (c->kind) {
        case K_HB:
        case K_HP:
            g = geminate(a, np, 1);
            if (g) { num->pron = g; shift_row(&o, c->pron, 1); }
            else if (ends_with(np, "サン") || ends_with(np, "セン") || ends_with(np, "ゼン") || ends_with(np, "マン")) {
                shift_row(&o, c->pron, c->kind == K_HP);
            } else if (c->kind == K_HP && ends_with(np, "ヨン")) {
                shift_row(&o, c->pron, 1);
            } else {
                cy_str_puts(&o, c->pron);
            }
            ctr->pron = cy_strndup(a, o.p, o.n);
            break;
        case K_K:
            if (c->k3voiced && ends_with(np, "サン")) {
                cy_str_puts(&o, strcmp(c->surf, "軒") == 0 ? "ゲン" : "ガイ");
                ctr->pron = cy_strndup(a, o.p, o.n);
            } else if ((g = geminate(a, np, 1)) != NULL) {
                num->pron = g;
            }
            break;
        case K_S:
        case K_T:
            if (c->k3voiced && (ends_with(np, "サン") || ends_with(np, "セン"))) {
                ctr->pron = "ゾク";
            }
            if (ends_with(np, "イチ") || ends_with(np, "ハチ") || ends_with(np, "ジュー")) {
                g = geminate(a, np, 0);
                if (g) num->pron = g;
            }
            break;
        case K_GATSU:
            if (v >= 1 && v <= 12) {
                if (last == 4 && v == 4) num->pron = "シ";
                else if (last == 7 && v == 7) num->pron = "シチ";
                else if (last == 9 && v == 9) num->pron = "ク";
            }
            break;
        case K_JI:
            if (ends_with(np, "ヨン")) num->pron = replace_tail(a, np, "ヨン", "ヨ");
            else if (ends_with(np, "ナナ")) num->pron = replace_tail(a, np, "ナナ", "シチ");
            else if (ends_with(np, "キュー")) num->pron = replace_tail(a, np, "キュー", "ク");
            break;
        case K_YO:
            if (ends_with(np, "ヨン")) num->pron = replace_tail(a, np, "ヨン", "ヨ");
            break;
        case K_NIN:
            if (v == 1) { num->pron = "ヒトリ"; ctr->pron = ""; set_mor(a, num, "ヒトリ", 2); }
            else if (v == 2) { num->pron = "フタリ"; ctr->pron = ""; set_mor(a, num, "フタリ", 3); }
            else if (ends_with(np, "ヨン")) num->pron = replace_tail(a, np, "ヨン", "ヨ");
            break;
        case K_NICHI: {
            static const char* DAYS[] = { NULL, NULL, "フツカ", "ミッカ", "ヨッカ", "イツカ", "ムイカ", "ナノカ",
                                          "ヨーカ", "ココノカ", "トーカ" };
            int date = i > 0 && strcmp(m[i - 1].surf, "月") == 0;
            const char* day = NULL;
            if (v == 1) {
                if (date) day = "ツイタチ";
            } else if (v >= 2 && v <= 10) {
                day = DAYS[v];
            } else if (v == 14) day = "ジューヨッカ";
            else if (v == 20) day = "ハツカ";
            else if (v == 24) day = "ニジューヨッカ";
            if (day) {
                set_mor(a, num, day, 0);   /* 日付の読みは平板 */
                ctr->pron = "";
            } else if (ends_with(np, "ヨン")) {
                num->pron = replace_tail(a, np, "ヨン", "ヨッ");
                ctr->pron = "カ";
            }
            break;
        }
        case K_TSU: {
            static const char* TSU[] = { NULL, "ヒトツ", "フタツ", "ミッツ", "ヨッツ", "イツツ", "ムッツ",
                                         "ナナツ", "ヤッツ", "ココノツ", "トー" };
            static const int TACC[] = { 0, 2, 3, 3, 3, 2, 3, 2, 3, 2, 1 };
            if (v >= 1 && v <= 10) { set_mor(a, num, TSU[v], TACC[v]); ctr->pron = ""; }
            break;
        }
        default:
            break;
        }
        /* 読みが変わったので数のモーラ数を合わせる（核の位置は数のまま） */
        if (num->pron && num->acc) {
            int acc = 0, mora = cy_count_mora(num->pron);
            char buf[16];
            sscanf(num->acc, "%d", &acc);
            if (acc > mora) acc = mora;
            snprintf(buf, sizeof buf, "%d/%d", acc, mora);
            num->acc = cy_strdup(a, buf);
        }
        if (ctr->pron) {
            char buf[16];
            int acc = 0;
            sscanf(ctr->acc ? ctr->acc : "0", "%d", &acc);
            snprintf(buf, sizeof buf, "%d/%d", acc, cy_count_mora(ctr->pron));
            ctr->acc = cy_strdup(a, buf);
        }
        cy_str_free(&o);
        i++;   /* 助数詞を飛ばす */
    }
}
