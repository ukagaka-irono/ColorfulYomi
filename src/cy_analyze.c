/* ColorfulYomi: 正規化と形態素解析（ラティスとビタビ探索）  SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cy_internal.h"

#define MAX_WORD_CHARS 64
#define MAX_GROUP_CHARS 256

/* ---- 正規化 ---- */
static const uint16_t HALF_KANA[] = {   /* U+FF61..U+FF9F */
    0x3002, 0x300C, 0x300D, 0x3001, 0x30FB, 0x30F2, 0x30A1, 0x30A3, 0x30A5, 0x30A7, 0x30A9, 0x30E3, 0x30E5,
    0x30E7, 0x30C3, 0x30FC, 0x30A2, 0x30A4, 0x30A6, 0x30A8, 0x30AA, 0x30AB, 0x30AD, 0x30AF, 0x30B1, 0x30B3,
    0x30B5, 0x30B7, 0x30B9, 0x30BB, 0x30BD, 0x30BF, 0x30C1, 0x30C4, 0x30C6, 0x30C8, 0x30CA, 0x30CB, 0x30CC,
    0x30CD, 0x30CE, 0x30CF, 0x30D2, 0x30D5, 0x30D8, 0x30DB, 0x30DE, 0x30DF, 0x30E0, 0x30E1, 0x30E2, 0x30E4,
    0x30E6, 0x30E8, 0x30E9, 0x30EA, 0x30EB, 0x30EC, 0x30ED, 0x30EF, 0x30F3, 0x309B, 0x309C
};

static int is_kana_like(uint32_t c)
{
    return (c >= 0x3041 && c <= 0x3096) || (c >= 0x30A1 && c <= 0x30FA) || c == 0x30FC;
}

/* 半角の濁点（hand=0）・半濁点（hand=1）を付けた全角カナ。付けられなければ 0 */
static uint32_t voice(uint32_t p, int hand)
{
    if (p >= 0x30CF && p <= 0x30DB && (p - 0x30CF) % 3 == 0) return p + (hand ? 2 : 1);   /* ハ行 */
    if (hand) return 0;
    if (p == 0x30A6) return 0x30F4;                                                         /* ウ → ヴ */
    if (p >= 0x30AB && p <= 0x30C1 && (p - 0x30AB) % 2 == 0) return p + 1;                 /* カ〜チ */
    if (p == 0x30C4 || p == 0x30C6 || p == 0x30C8) return p + 1;                            /* ツテト */
    return 0;
}

int cy_normalize(const char* in, size_t len, CyStr* out)
{
    size_t i = 0;
    uint32_t c, last = 0;
    while (i < len) {
        if (!cy_utf8_next(in, len, &i, &c)) return CT_ERR_UTF8;
        if (c == 0xFEFF) continue;
        if (c == '\t' || c == '\r' || c == '\n' || c == 0x3000) c = ' ';
        else if (c < 0x20 || c == 0x7F) continue;
        else if (c >= 0x21 && c <= 0x7E) c += 0xFEE0;
        else if (c >= 0xFF61 && c <= 0xFF9F) {
            c = HALF_KANA[c - 0xFF61];
            if (c == 0x309B || c == 0x309C) {           /* 半角の濁点・半濁点を前の字に付ける */
                uint32_t nc = voice(last, c == 0x309C);
                if (nc) {
                    /* 直前に出した 1 文字（3 バイト）を置き換える */
                    out->n -= 3;
                    cy_str_putcp(out, nc);
                    last = nc;
                }
                continue;
            }
        }
        if ((c == 0x301C || c == 0xFF5E) && is_kana_like(last)) c = 0x30FC;   /* 〜 → ー */
        cy_str_putcp(out, c);
        last = c;
    }
    return out->oom ? CT_ERR_MEMORY : CT_OK;
}

/* ---- ラティス ---- */
typedef struct {
    int b, e;
    int left, right, cost;
    long long total;
    int prev;
    int kind;
    long ref;
    int next_end;
} Node;

typedef struct {
    Node* v; size_t n, cap;
    int oom;
} Nodes;

static int push(Nodes* ns, const Node* x)
{
    if (ns->n == ns->cap) {
        size_t cap = ns->cap ? ns->cap * 2 : 1024;
        Node* p = (Node*)realloc(ns->v, cap * sizeof *p);
        if (!p) { ns->oom = 1; return -1; }
        ns->v = p; ns->cap = cap;
    }
    ns->v[ns->n] = *x;
    return (int)ns->n++;
}

typedef struct {
    const CyDic* d;
    Nodes ns;
    int* end_head;
    const char* text; const size_t* boff; const uint32_t* cps; int n;
} Lat;

/* 位置 b で終わる最良の前の節点を選んで登録する */
static void add_node(Lat* L, int b, int e, int left, int right, int cost, int kind, long ref)
{
    Node x;
    int k, best = -1, idx;
    long long bt = 0;
    for (k = L->end_head[b]; k >= 0; k = L->ns.v[k].next_end) {
        long long t = L->ns.v[k].total + cy_conn_cost(L->d, L->ns.v[k].right, left);
        if (best < 0 || t < bt) { best = k; bt = t; }
    }
    if (best < 0) return;
    x.b = b; x.e = e; x.left = left; x.right = right; x.cost = cost;
    x.total = bt + cost; x.prev = best; x.kind = kind; x.ref = ref;
    x.next_end = -1;
    idx = push(&L->ns, &x);
    if (idx < 0) return;
    L->ns.v[idx].next_end = L->end_head[e];
    L->end_head[e] = idx;
}

static void add_unknown(Lat* L, int b, int e, int cat)
{
    const CyDic* d = L->d;
    uint32_t i;
    int any = 0;
    for (i = 0; i < d->nunk; i++)
        if (d->unk[i].cat == cat) {
            add_node(L, b, e, d->unk[i].left, d->unk[i].right, d->unk[i].cost, MK_UNK, (long)i);
            any = 1;
        }
    if (!any && cat != d->cat_default) add_unknown(L, b, e, d->cat_default);
}

/* 数字の並び（全角に正規化済み）: 1,234,567.89 の形。終わりの位置を返す */
static int scan_number(const uint32_t* c, int n, int i)
{
#define DIG(k) ((k) < n && c[k] >= 0xFF10 && c[k] <= 0xFF19)
    int j = i, lead;
    while (DIG(j)) j++;
    lead = j - i;
    if (lead <= 3) {
        while (j + 3 < n + 0 && c[j] == 0xFF0C && DIG(j + 1) && DIG(j + 2) && DIG(j + 3) && !DIG(j + 4)) j += 4;
    }
    if (j + 1 < n && c[j] == 0xFF0E && DIG(j + 1)) {
        j++;
        while (DIG(j)) j++;
    }
    return j;
#undef DIG
}

static const char* ALPHA_NAME[26] = {
    "エー", "ビー", "シー", "ディー", "イー", "エフ", "ジー", "エイチ", "アイ", "ジェー", "ケー", "エル", "エム",
    "エヌ", "オー", "ピー", "キュー", "アール", "エス", "ティー", "ユー", "ブイ", "ダブリュー", "エックス", "ワイ", "ゼット"
};

/* 未知語の読みとアクセント */
static void unknown_reading(const CyDic* d, CyArena* a, CyMor* m)
{
    CyStr s = { 0 };
    size_t i = 0;
    uint32_t c;
    int mora, acc = 0, last_letter_start = 0;
    char buf[16];
    if (m->cat == d->cat_kata || m->cat == d->cat_hira) {
        while (cy_utf8_next(m->surf, m->slen, &i, &c)) cy_str_putcp(&s, cy_to_katakana(c));
        mora = s.n ? cy_count_mora(s.p) : 0;
        if (m->cat == d->cat_kata && mora > 0) acc = mora >= 3 ? mora - 2 : 1;  /* 外来語: 後ろから 3 つ目 */
    } else if (m->cat == d->cat_alpha) {
        while (cy_utf8_next(m->surf, m->slen, &i, &c)) {
            int k = -1;
            if (c >= 0xFF21 && c <= 0xFF3A) k = (int)(c - 0xFF21);
            else if (c >= 0xFF41 && c <= 0xFF5A) k = (int)(c - 0xFF41);
            if (k < 0) continue;
            last_letter_start = s.n ? cy_count_mora(s.p) : 0;
            cy_str_puts(&s, ALPHA_NAME[k]);
        }
        mora = s.n ? cy_count_mora(s.p) : 0;
        if (mora > 0) acc = last_letter_start + 1;   /* 最後の文字の 1 モーラ目 */
    } else {
        mora = 0;
    }
    m->pron = s.n ? cy_strndup(a, s.p, s.n) : "";
    snprintf(buf, sizeof buf, "%d/%d", acc, mora);
    m->acc = cy_strdup(a, buf);
    m->chain = "*";
    cy_str_free(&s);
}

int cy_analyze(const CyDic* d, const char* text, CyArena* a, CyMor** out, size_t* nout)
{
    CyStr norm = { 0 };
    Lat L;
    uint32_t* cps = NULL;
    size_t* boff = NULL;
    int* numend = NULL;
    int n = 0, i, r, eos_best = -1, cnt, k;
    long long eos_t = 0;
    size_t bi;
    CyMor* ms;
    Node bos;

    *out = NULL; *nout = 0;
    memset(&L, 0, sizeof L);
    r = cy_normalize(text, strlen(text), &norm);
    if (r != CT_OK) { cy_str_free(&norm); return r; }
    if (norm.n == 0) { cy_str_free(&norm); return CT_OK; }

    cps = (uint32_t*)malloc((norm.n + 1) * sizeof *cps);
    boff = (size_t*)malloc((norm.n + 1) * sizeof *boff);
    numend = (int*)malloc((norm.n + 1) * sizeof *numend);
    L.end_head = (int*)malloc((norm.n + 1) * sizeof *L.end_head);
    if (!cps || !boff || !numend || !L.end_head) { r = CT_ERR_MEMORY; goto done; }
    bi = 0;
    while (bi < norm.n) {
        boff[n] = bi;
        cy_utf8_next(norm.p, norm.n, &bi, &cps[n]);
        n++;
    }
    boff[n] = norm.n;
    for (i = 0; i <= n; i++) { L.end_head[i] = -1; numend[i] = -1; }
    for (i = 0; i < n; ) {
        if (cps[i] >= 0xFF10 && cps[i] <= 0xFF19) { numend[i] = scan_number(cps, n, i); i = numend[i]; }
        else i++;
    }
    L.d = d; L.text = norm.p; L.boff = boff; L.cps = cps; L.n = n;

    memset(&bos, 0, sizeof bos);
    bos.prev = -1; bos.next_end = -1;
    push(&L.ns, &bos);
    L.end_head[0] = 0;

    {
        int inside_until = 0, next_num = 0;
        for (i = 0; i < n; i++) {
            int limit, e, found = 0, prim, ex;
            uint32_t mask;
            const CyCat* cat;
            if (L.end_head[i] < 0) continue;
            if (numend[i] >= 0) {
                /* 数は 1 語として扱う（名詞,数 の文脈） */
                int ci;
                inside_until = numend[i];
                for (ci = 0; ci < (int)d->nunk; ci++)
                    if (d->unk[ci].cat == d->cat_numeric) break;
                if (ci < (int)d->nunk)
                    add_node(&L, i, numend[i], d->unk[ci].left, d->unk[ci].right, -20000, MK_NUMBER, 0);
                else
                    add_node(&L, i, numend[i], 0, 0, -20000, MK_NUMBER, 0);
                continue;
            }
            if (i < inside_until) continue;
            /* 次の数の始まりより先へは伸ばさない */
            if (next_num <= i) {
                next_num = i + 1;
                while (next_num < n && numend[next_num] < 0) next_num++;
            }
            limit = next_num;
            if (limit - i > MAX_WORD_CHARS) limit = i + MAX_WORD_CHARS;
            for (e = i + 1; e <= limit; e++) {
                long s = cy_find_surface(d, norm.p + boff[i], boff[e] - boff[i], &ex);
                if (s >= 0) {
                    const CySurf* sf = &d->surf[s];
                    uint32_t w;
                    for (w = sf->first; w < sf->first + sf->n; w++)
                        add_node(&L, i, e, d->word[w].left, d->word[w].right, d->word[w].cost, MK_WORD, (long)w);
                    found = 1;
                }
                if (!ex) break;
            }
            if (d->nuser) {
                for (e = i + 1; e <= limit; e++) {
                    long s = cy_find_user(d, norm.p + boff[i], boff[e] - boff[i], &ex);
                    if (s >= 0) {
                        add_node(&L, i, e, d->user[s].left, d->user[s].right, d->user[s].cost, MK_USER, s);
                        found = 1;
                    }
                    if (!ex) break;
                }
            }
            /* 未知語 */
            cy_char_info(d, cps[i], &prim, &mask);
            cat = &d->cat[prim];
            if (cat->invoke || !found) {
                int gend = -1, len;
                if (cat->group) {
                    int j = i + 1, p2;
                    uint32_t m2;
                    int glim = next_num < n ? next_num : n;
                    while (j < glim && j - i < MAX_GROUP_CHARS) {
                        cy_char_info(d, cps[j], &p2, &m2);
                        if (!(m2 & (1u << prim))) break;
                        j++;
                    }
                    gend = j;
                    add_unknown(&L, i, j, prim);
                }
                for (len = 1; len <= cat->length; len++) {
                    int p2, ok = 1, j;
                    uint32_t m2;
                    if (i + len > limit) break;
                    for (j = i + 1; j < i + len; j++) {
                        cy_char_info(d, cps[j], &p2, &m2);
                        if (!(m2 & (1u << prim))) { ok = 0; break; }
                    }
                    if (!ok) break;
                    if (i + len == gend) continue;
                    add_unknown(&L, i, i + len, prim);
                }
                if (!cat->group && cat->length == 0) add_unknown(&L, i, i + 1, prim);
            }
            if (L.ns.oom) { r = CT_ERR_MEMORY; goto done; }
        }
    }
    for (k = L.end_head[n]; k >= 0; k = L.ns.v[k].next_end) {
        long long t = L.ns.v[k].total + cy_conn_cost(d, L.ns.v[k].right, 0);
        if (eos_best < 0 || t < eos_t) { eos_best = k; eos_t = t; }
    }
    if (eos_best < 0) { r = CT_ERR_MEMORY; goto done; }   /* 起こらないはず */

    cnt = 0;
    for (k = eos_best; k > 0; k = L.ns.v[k].prev) cnt++;
    ms = (CyMor*)cy_alloc(a, (size_t)(cnt ? cnt : 1) * sizeof *ms);
    if (!ms) { r = CT_ERR_MEMORY; goto done; }
    i = cnt;
    for (k = eos_best; k > 0; k = L.ns.v[k].prev) {
        const Node* x = &L.ns.v[k];
        CyMor* m = &ms[--i];
        memset(m, 0, sizeof *m);
        m->kind = x->kind;
        m->surf = cy_strndup(a, norm.p + boff[x->b], boff[x->e] - boff[x->b]);
        m->slen = boff[x->e] - boff[x->b];
        m->cat = -1;
        switch (x->kind) {
        case MK_WORD: {
            const CyWord* w = &d->word[x->ref];
            m->pos = cy_pool(d, d->pos[w->pos]);
            m->pron = cy_pool(d, w->pron);
            m->acc = cy_pool(d, w->acc);
            m->chain = cy_pool(d, w->chain);
            break;
        }
        case MK_USER: {
            const CyUser* u = &d->user[x->ref];
            m->pos = u->pos; m->pron = u->pron; m->acc = u->acc; m->chain = "*";
            break;
        }
        case MK_UNK: {
            const CyUnk* u = &d->unk[x->ref];
            m->pos = cy_pool(d, d->pos[u->pos]);
            m->cat = u->cat;
            unknown_reading(d, a, m);
            break;
        }
        default: {   /* 数 */
            char* dg = (char*)cy_alloc(a, (size_t)(x->e - x->b) + 1);
            char buf[16];
            int j, nd = 0, acc = 0;
            if (!dg) { r = CT_ERR_MEMORY; goto done; }
            for (j = x->b; j < x->e; j++) {
                if (cps[j] >= 0xFF10 && cps[j] <= 0xFF19) dg[nd++] = (char)('0' + (cps[j] - 0xFF10));
                else if (cps[j] == 0xFF0E) dg[nd++] = '.';
            }
            dg[nd] = '\0';
            m->digits = dg; m->dlen = nd;
            m->pos = "名詞,数,*,*,*,*";
            {
                /* ハイフンでつながった数字の組（090-1234-5678）は 1 けたずつ */
                int hy = (x->b >= 2 && cps[x->b - 1] == 0xFF0D && cps[x->b - 2] >= 0xFF10 && cps[x->b - 2] <= 0xFF19) ||
                         (x->e + 1 < n && cps[x->e] == 0xFF0D && cps[x->e + 1] >= 0xFF10 && cps[x->e + 1] <= 0xFF19);
                m->pron = cy_number_reading(a, dg, nd, hy, &acc);
            }
            snprintf(buf, sizeof buf, "%d/%d", acc, m->pron ? cy_count_mora(m->pron) : 0);
            m->acc = cy_strdup(a, buf);
            m->chain = "*";
            break;
        }
        }
    }
    if (a->oom) { r = CT_ERR_MEMORY; goto done; }
    *out = ms; *nout = (size_t)cnt;
    r = CT_OK;
done:
    free(L.ns.v); free(L.end_head);
    free(cps); free(boff); free(numend);
    cy_str_free(&norm);
    return r;
}
