/*
 * ColorfulTalk 共通定義（エラーコード・呼出規約）
 * SPDX-License-Identifier: MIT
 */
#ifndef COLORFUL_TALK_COMMON_H
#define COLORFUL_TALK_COMMON_H

#if defined(_WIN32) && defined(CT_BUILD_DLL)
#  define CT_API __declspec(dllexport)
#elif defined(_WIN32) && defined(CT_USE_DLL)
#  define CT_API __declspec(dllimport)
#elif defined(__GNUC__) && defined(CT_BUILD_DLL)
#  define CT_API __attribute__((visibility("default")))
#else
#  define CT_API
#endif

/* エラーコード（全エンジン共通） */
enum {
    CT_OK          = 0,
    CT_ERR_NULL    = 1,   /* 引数が NULL */
    CT_ERR_UTF8    = 2,   /* 不正な UTF-8 */
    CT_ERR_CHAR    = 3,   /* 記号列に使えない文字 */
    CT_ERR_ACCENT  = 4,   /* アクセント記号の位置が不正 */
    CT_ERR_LONG    = 5,   /* 長音記号の位置が不正 */
    CT_ERR_DEVOICE = 6,   /* 無声化記号の位置が不正 */
    CT_ERR_EMPTY   = 7,   /* 発音する文字がない */
    CT_ERR_PARAM   = 8,   /* パラメータが範囲外 */
    CT_ERR_TOOLONG = 9,   /* 記号列が長すぎる */
    CT_ERR_MEMORY  = 10,  /* メモリ不足 */
    CT_ERR_VOICE   = 11   /* 声データが不正 */
};

#endif
