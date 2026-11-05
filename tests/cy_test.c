/* ColorfulYomi の単体試験  SPDX-License-Identifier: MIT
 * 辞書なしで試せる部分（数の読み・ローマ字変換）と、環境変数 CY_DIC があれば辞書を使う試験 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cy_internal.h"

static int fails = 0;

static void expect(const char* what, const char* got, const char* want)
{
    if (strcmp(got, want) != 0) {
        printf("NG %s: got [%s] want [%s]\n", what, got, want);
        fails++;
    }
}

static void test_number(const char* digits, const char* want, int want_acc)
{
    CyArena a = { 0 };
    int acc = -1;
    const char* r = cy_number_reading(&a, digits, (int)strlen(digits), 0, &acc);
    expect(digits, r, want);
    if (want_acc >= 0 && acc != want_acc) { printf("NG %s accent: got %d want %d\n", digits, acc, want_acc); fails++; }
    cy_arena_free(&a);
}

static void test_roman(const char* koe, const char* want)
{
    CyStr s = { 0 };
    cy_koe_to_roman(koe, &s);
    expect(koe, s.p ? s.p : "", want);
    cy_str_free(&s);
}

static void test_koe(const CyDic* d, const char* text, const char* want)
{
    char buf[1024];
    int r = cy_text_to_koe(d, text, buf, sizeof buf, NULL);
    if (r != CT_OK) { printf("NG %s: error %d\n", text, r); fails++; return; }
    expect(text, buf, want);
}

int main(void)
{
    const char* dic = getenv("CY_DIC");
    test_number("0", "ゼロ", -1);
    test_number("3", "サン", 0);
    test_number("10", "ジュー", 1);
    test_number("23", "ニジューサン", 4);
    test_number("300", "サンビャク", 3);
    test_number("2026", "ニセンニジューロク", 8);
    test_number("10000", "イチマン", 3);
    test_number("12345678", "センニヒャクサンジューヨンマンゴセンロッピャクナナジューハチ", -1);
    test_number("100000000", "イチオク", 3);
    test_number("090", "ゼロキューゼロ", 0);
    test_number("3.14", "サンテンイチヨン", 3);

    test_roman("こんにちわ、せか'い。", "koNnichiwa,seka'i.");
    test_roman("きょ'ーわ/い'い/てんきです。", "kyo'-wa/i'i/teNkidesu.");
    test_roman("がっこー", "gakko-");
    test_roman("まっちゃ", "maccha");
    test_roman("いっぬ", "iqnu");
    test_roman("ふぁいる", "fairu");

    if (dic) {
        CyDic* d;
        int r = cy_open(dic, &d);
        if (r != CT_OK) { printf("NG open %s: %d\n", dic, r); return 1; }
        test_koe(d, "", "");
        test_koe(d, "。", "");
        test_koe(d, "東京", "とーきょー");
        test_koe(d, "学生です。", "がくせいで'す。");
        test_koe(d, "鉛筆が3本あります。", "えんぴつが/さ'んぼん/ありま'す。");
        test_koe(d, "1本", "い'っぽん");
        test_koe(d, "2026年11月5日です。", "にせんにじゅーろくねん/じゅーいち'がつ/いつかで'す。");
        test_koe(d, "ＸＹＺ", "えっくすわいぜ'っと");
        r = cy_add_user_word(d, "ColorfulTalk", "カラフルトーク", 5, "固有名詞");
        if (r != CT_OK) { printf("NG user word: %d\n", r); fails++; }
        test_koe(d, "ColorfulTalkです", "からふると'ーくです");
        cy_close(d);
    } else {
        printf("CY_DIC がないので辞書を使う試験は省きました\n");
    }
    printf(fails ? "%d 件の失敗\n" : "すべて合格\n", fails);
    return fails != 0;
}
