/* ColorfulYomi: 公開関数  SPDX-License-Identifier: MIT */
#include <string.h>
#include "cy_internal.h"

static int copy_out(const CyStr* s, char* out, size_t out_size, size_t* need)
{
    size_t n = s->n + 1;
    if (need) *need = n;
    if (!out || out_size < n) {
        if (out && out_size) out[0] = '\0';
        return CT_ERR_BUFFER;
    }
    if (s->n) memcpy(out, s->p, s->n);
    out[s->n] = '\0';
    return CT_OK;
}

enum { OUT_KOE, OUT_ROMAN, OUT_MORPHS };

static int convert(const CyDic* d, const char* text, char* out, size_t out_size, size_t* need, int mode)
{
    CyArena a = { 0 };
    CyStr koe = { 0 }, res = { 0 };
    CyMor* m = NULL;
    size_t n = 0, i;
    int r;
    if (need) *need = 0;
    if (!d || !text) return CT_ERR_NULL;
    r = cy_analyze(d, text, &a, &m, &n);
    if (r == CT_OK) cy_apply_counters(&a, m, n);
    if (r == CT_OK && a.oom) r = CT_ERR_MEMORY;
    if (r == CT_OK) {
        if (mode == OUT_MORPHS) {
            for (i = 0; i < n; i++) {
                cy_str_putn(&res, m[i].surf, m[i].slen);
                cy_str_putc(&res, '\t'); cy_str_puts(&res, m[i].pos ? m[i].pos : "");
                cy_str_putc(&res, '\t'); cy_str_puts(&res, m[i].pron ? m[i].pron : "");
                cy_str_putc(&res, '\t'); cy_str_puts(&res, m[i].acc ? m[i].acc : "");
                cy_str_putc(&res, '\t'); cy_str_puts(&res, m[i].chain ? m[i].chain : "");
                cy_str_putc(&res, '\n');
            }
            if (!res.p) cy_str_puts(&res, "");
        } else {
            r = cy_build_koe(m, n, &koe);
            if (r == CT_OK) {
                if (mode == OUT_ROMAN) r = cy_koe_to_roman(koe.p ? koe.p : "", &res);
                else cy_str_puts(&res, koe.p ? koe.p : "");
            }
        }
        if (r == CT_OK && res.oom) r = CT_ERR_MEMORY;
        if (r == CT_OK) r = copy_out(&res, out, out_size, need);
    }
    cy_str_free(&koe);
    cy_str_free(&res);
    cy_arena_free(&a);
    return r;
}

int cy_text_to_koe(const CyDic* d, const char* t, char* o, size_t s, size_t* need) { return convert(d, t, o, s, need, OUT_KOE); }
int cy_text_to_roman(const CyDic* d, const char* t, char* o, size_t s, size_t* need) { return convert(d, t, o, s, need, OUT_ROMAN); }
int cy_text_to_morphs(const CyDic* d, const char* t, char* o, size_t s, size_t* need) { return convert(d, t, o, s, need, OUT_MORPHS); }
