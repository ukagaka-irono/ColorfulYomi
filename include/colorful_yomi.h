/*
 * ColorfulYomi: 漢字かな交じり文を ColorfulTalk の記号列に変換する言語処理エンジン
 * SPDX-License-Identifier: MIT
 *
 * 使い方:
 *   CyDic* dic;
 *   cy_open("colorful_talk.dic", &dic);
 *   char koe[4096];
 *   cy_text_to_koe(dic, "今日はいい天気です。", koe, sizeof koe, NULL);
 *   // koe = "きょ'ーわ/い'い/てんきです。" → ct3_start() などに渡す
 *   cy_close(dic);
 *
 * 辞書は読み込んだ後は変更しないので（cy_add_user_dic を除く）、
 * 1 つの CyDic を複数のスレッドから同時に cy_text_to_* に使えます。
 */
#ifndef COLORFUL_YOMI_H
#define COLORFUL_YOMI_H

#include <stddef.h>
#include "colorful_talk_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CY_VERSION "1.0.0"

/* ColorfulYomi が加えるエラーコード（値は全製品共通） */
#ifndef CT_COMMON_YOMI_ERRORS
#define CT_COMMON_YOMI_ERRORS
enum {
    CT_ERR_IO     = 12,  /* ファイルを読めない */
    CT_ERR_DIC    = 13,  /* 辞書の形式が違う・壊れている */
    CT_ERR_BUFFER = 14   /* 出力先のバッファが足りない */
};
#endif

typedef struct CyDic CyDic;

/* 版の文字列（"1.0.0"） */
CT_API const char* cy_version(void);

/* 辞書ファイル（colorful_talk.dic）を読み込む。パスは UTF-8 */
CT_API int  cy_open(const char* dic_path_utf8, CyDic** out);

/* メモリ上の辞書を使う。data は cy_close まで有効にしておくこと（コピーしない） */
CT_API int  cy_open_memory(const void* data, size_t size, CyDic** out);

CT_API void cy_close(CyDic* dic);

/* ユーザー辞書（CSV: 表層形,読み,アクセント型,品詞）を追加する。
 * *error_line に誤りのある行番号（1 始まり）を返す（NULL 可）。
 * 他のスレッドが同じ辞書で変換している間は呼ばないこと */
CT_API int  cy_add_user_dic(CyDic* dic, const char* csv_path_utf8, int* error_line);
CT_API int  cy_add_user_word(CyDic* dic, const char* surface, const char* reading, int accent, const char* pos);

/* 漢字かな交じり文（UTF-8）を記号列に変換する。
 * 出力は NUL 終端。out_size が足りなければ CT_ERR_BUFFER を返し、*need に必要なバイト数（NUL を含む）を入れる。
 * need は NULL 可。out は NULL・out_size は 0 でもよい（必要な大きさだけ調べる） */
CT_API int  cy_text_to_koe(const CyDic* dic, const char* text_utf8, char* out, size_t out_size, size_t* need);   /* カナ記号列（V1/V2/V3） */
CT_API int  cy_text_to_roman(const CyDic* dic, const char* text_utf8, char* out, size_t out_size, size_t* need); /* ローマ字記号列（nano / pico LSI） */

/* 調べる用: 形態素解析の結果を 1 行 1 語（表層形 TAB 品詞 TAB 発音 TAB アクセント TAB 結合規則）で返す */
CT_API int  cy_text_to_morphs(const CyDic* dic, const char* text_utf8, char* out, size_t out_size, size_t* need);

#ifdef __cplusplus
}
#endif

#endif
