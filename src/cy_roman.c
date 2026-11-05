/* ColorfulYomi: カナ記号列 → ローマ字記号列（ColorfulTalk nano / pico LSI）  SPDX-License-Identifier: MIT */
#include <string.h>
#include "cy_internal.h"

typedef struct { const char* kana; const char* roman; } KR;

/* 2 文字の組み合わせを先に調べる */
static const KR PAIRS[] = {
    {"きゃ","kya"},{"きゅ","kyu"},{"きょ","kyo"},{"ぎゃ","gya"},{"ぎゅ","gyu"},{"ぎょ","gyo"},
    {"しゃ","sha"},{"しゅ","shu"},{"しょ","sho"},{"しぇ","she"},{"じゃ","ja"},{"じゅ","ju"},{"じょ","jo"},{"じぇ","je"},
    {"ちゃ","cha"},{"ちゅ","chu"},{"ちょ","cho"},{"ちぇ","che"},{"ぢゃ","ja"},{"ぢゅ","ju"},{"ぢょ","jo"},{"ぢぇ","je"},
    {"にゃ","nya"},{"にゅ","nyu"},{"にょ","nyo"},{"ひゃ","hya"},{"ひゅ","hyu"},{"ひょ","hyo"},
    {"びゃ","bya"},{"びゅ","byu"},{"びょ","byo"},{"ぴゃ","pya"},{"ぴゅ","pyu"},{"ぴょ","pyo"},
    {"みゃ","mya"},{"みゅ","myu"},{"みょ","myo"},{"りゃ","rya"},{"りゅ","ryu"},{"りょ","ryo"},
    {"ふぁ","fa"},{"ふぃ","fi"},{"ふぇ","fe"},{"ふぉ","fo"},{"ふゅ","fyu"},
    {"てぃ","thi"},{"でぃ","dhi"},{"とぅ","twu"},{"どぅ","dwu"},
    {"つぁ","tsa"},{"つぃ","tsi"},{"つぇ","tse"},{"つぉ","tso"},
    {"うぃ","wi"},{"うぇ","we"},{"うぉ","wo"},{"いぇ","ye"},
    {"ゔぁ","va"},{"ゔぃ","vi"},{"ゔぇ","ve"},{"ゔぉ","vo"},{"ゔゅ","byu"},
};

static const KR SINGLE[] = {
    {"あ","a"},{"い","i"},{"う","u"},{"え","e"},{"お","o"},
    {"か","ka"},{"き","ki"},{"く","ku"},{"け","ke"},{"こ","ko"},{"が","ga"},{"ぎ","gi"},{"ぐ","gu"},{"げ","ge"},{"ご","go"},
    {"さ","sa"},{"し","shi"},{"す","su"},{"せ","se"},{"そ","so"},{"ざ","za"},{"じ","ji"},{"ず","zu"},{"ぜ","ze"},{"ぞ","zo"},
    {"た","ta"},{"ち","chi"},{"つ","tsu"},{"て","te"},{"と","to"},{"だ","da"},{"ぢ","di"},{"づ","du"},{"で","de"},{"ど","do"},
    {"な","na"},{"に","ni"},{"ぬ","nu"},{"ね","ne"},{"の","no"},
    {"は","ha"},{"ひ","hi"},{"ふ","fu"},{"へ","he"},{"ほ","ho"},{"ば","ba"},{"び","bi"},{"ぶ","bu"},{"べ","be"},{"ぼ","bo"},
    {"ぱ","pa"},{"ぴ","pi"},{"ぷ","pu"},{"ぺ","pe"},{"ぽ","po"},
    {"ま","ma"},{"み","mi"},{"む","mu"},{"め","me"},{"も","mo"},{"や","ya"},{"ゆ","yu"},{"よ","yo"},
    {"ら","ra"},{"り","ri"},{"る","ru"},{"れ","re"},{"ろ","ro"},{"わ","wa"},{"を","wo"},{"ゔ","vu"},
    {"ぁ","a"},{"ぃ","i"},{"ぅ","u"},{"ぇ","e"},{"ぉ","o"},{"ゃ","ya"},{"ゅ","yu"},{"ょ","yo"},{"ゎ","wa"},
    {"ゐ","i"},{"ゑ","e"},
};

static const char* lookup(const KR* t, size_t n, const char* p, size_t len)
{
    size_t i;
    for (i = 0; i < n; i++)
        if (strlen(t[i].kana) == len && memcmp(t[i].kana, p, len) == 0) return t[i].roman;
    return NULL;
}

/* p から 1 モーラ分のローマ字を返す。*used に使ったバイト数 */
static const char* mora_at(const char* p, size_t rest, size_t* used)
{
    const char* r;
    if (rest >= 6 && (r = lookup(PAIRS, sizeof PAIRS / sizeof PAIRS[0], p, 6)) != NULL) { *used = 6; return r; }
    if (rest >= 3 && (r = lookup(SINGLE, sizeof SINGLE / sizeof SINGLE[0], p, 3)) != NULL) { *used = 3; return r; }
    return NULL;
}

int cy_koe_to_roman(const char* koe, CyStr* o)
{
    size_t i = 0, len = strlen(koe);
    while (i < len) {
        const char* p = koe + i;
        size_t used = 0;
        const char* r;
        if (*p == '\'' || *p == '/' || *p == '_') { cy_str_putc(o, *p); i++; continue; }
        if (len - i >= 3) {
            if (memcmp(p, "、", 3) == 0) { cy_str_putc(o, ','); i += 3; continue; }
            if (memcmp(p, "。", 3) == 0) { cy_str_putc(o, '.'); i += 3; continue; }
            if (memcmp(p, "！", 3) == 0) { cy_str_putc(o, '!'); i += 3; continue; }
            if (memcmp(p, "？", 3) == 0) { cy_str_putc(o, '?'); i += 3; continue; }
            if (memcmp(p, "ー", 3) == 0) { cy_str_putc(o, '-'); i += 3; continue; }
            if (memcmp(p, "ん", 3) == 0) { cy_str_putc(o, 'N'); i += 3; continue; }
            if (memcmp(p, "っ", 3) == 0) {
                /* 次のモーラの子音を重ねる。母音・な行・記号の前は q */
                size_t j = i + 3, u2;
                const char* nx;
                while (j < len && (koe[j] == '\'' || koe[j] == '_')) j++;
                nx = j < len ? mora_at(koe + j, len - j, &u2) : NULL;
                if (nx && !strchr("aiueon", nx[0])) cy_str_putc(o, nx[0]);
                else cy_str_putc(o, 'q');
                i += 3;
                continue;
            }
        }
        r = mora_at(p, len - i, &used);
        if (r) { cy_str_puts(o, r); i += used; continue; }
        /* 知らない文字は飛ばす */
        {
            uint32_t c;
            size_t k = i;
            if (!cy_utf8_next(koe, len, &k, &c)) return CT_ERR_UTF8;
            i = k;
        }
    }
    return o->oom ? CT_ERR_MEMORY : CT_OK;
}
