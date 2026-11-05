/* cyomi: 漢字かな交じり文を記号列に変換するコマンド  SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "colorful_yomi.h"

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <shellapi.h>
#  include <io.h>
#  include <fcntl.h>
#endif

static void usage(void)
{
    fputs("usage: cyomi [options] [TEXT...]\n"
          "  -d DIC     dictionary (default: $CY_DIC or colorful_talk.dic beside cyomi)\n"
          "  -u CSV     add a user dictionary (repeatable)\n"
          "  -r         output the romaji koe string (nano / pico LSI)\n"
          "  -m         output morphemes (for debugging)\n"
          "  -v         print the version\n"
          "Without TEXT, each line of standard input is converted.\n"
          "example: cyomi \"今日はいい天気です。\"\n", stderr);
}

static char** utf8_argv(int* argc, char** argv)
{
#ifdef _WIN32
    int n, i;
    LPWSTR* w = CommandLineToArgvW(GetCommandLineW(), &n);
    char** out;
    if (!w) return argv;
    out = (char**)calloc((size_t)n + 1, sizeof *out);
    for (i = 0; i < n; i++) {
        int len = WideCharToMultiByte(CP_UTF8, 0, w[i], -1, NULL, 0, NULL, NULL);
        out[i] = (char*)malloc((size_t)len);
        WideCharToMultiByte(CP_UTF8, 0, w[i], -1, out[i], len, NULL, NULL);
    }
    LocalFree(w);
    *argc = n;
    return out;
#else
    (void)argc;
    return argv;
#endif
}

static int convert(const CyDic* dic, const char* text, int mode)
{
    size_t need = 0;
    char* buf;
    int r;
    if (mode == 'r') r = cy_text_to_roman(dic, text, NULL, 0, &need);
    else if (mode == 'm') r = cy_text_to_morphs(dic, text, NULL, 0, &need);
    else r = cy_text_to_koe(dic, text, NULL, 0, &need);
    if (r != CT_ERR_BUFFER) { fprintf(stderr, "cyomi: error %d\n", r); return 1; }
    buf = (char*)malloc(need);
    if (!buf) return 1;
    if (mode == 'r') r = cy_text_to_roman(dic, text, buf, need, NULL);
    else if (mode == 'm') r = cy_text_to_morphs(dic, text, buf, need, NULL);
    else r = cy_text_to_koe(dic, text, buf, need, NULL);
    if (r == CT_OK) {
        fputs(buf, stdout);
        if (mode != 'm') fputc('\n', stdout);
    } else {
        fprintf(stderr, "cyomi: error %d\n", r);
    }
    free(buf);
    return r != CT_OK;
}

static const char* default_dic(const char* argv0)
{
    static char path[2048];
    const char* env = getenv("CY_DIC");
    const char* slash;
    if (env && *env) return env;
    slash = strrchr(argv0, '/');
#ifdef _WIN32
    {
        const char* bs = strrchr(argv0, '\\');
        if (!slash || (bs && bs > slash)) slash = bs;
    }
#endif
    if (slash && (size_t)(slash - argv0) + 32 < sizeof path) {
        FILE* f;
        memcpy(path, argv0, (size_t)(slash - argv0 + 1));
        strcpy(path + (slash - argv0 + 1), "colorful_talk.dic");
        f = fopen(path, "rb");
        if (f) { fclose(f); return path; }
    }
    return "colorful_talk.dic";
}

int main(int argc, char** argv)
{
    const char* dicpath = NULL;
    const char* users[16];
    int nusers = 0, mode = 'k', i, first = -1, err = 0, r;
    CyDic* dic;
    argv = utf8_argv(&argc, argv);
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) dicpath = argv[++i];
        else if (strcmp(argv[i], "-u") == 0 && i + 1 < argc) { if (nusers < 16) users[nusers++] = argv[++i]; }
        else if (strcmp(argv[i], "-r") == 0) mode = 'r';
        else if (strcmp(argv[i], "-m") == 0) mode = 'm';
        else if (strcmp(argv[i], "-v") == 0) { printf("ColorfulYomi %s\n", cy_version()); return 0; }
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) { usage(); return 0; }
        else { first = i; break; }
    }
    if (!dicpath) dicpath = default_dic(argv[0]);
    r = cy_open(dicpath, &dic);
    if (r != CT_OK) { fprintf(stderr, "cyomi: cannot open %s (error %d)\n", dicpath, r); return 2; }
    for (i = 0; i < nusers; i++) {
        int line = 0;
        r = cy_add_user_dic(dic, users[i], &line);
        if (r != CT_OK) { fprintf(stderr, "cyomi: %s line %d: error %d\n", users[i], line, r); cy_close(dic); return 2; }
    }
    if (first >= 0) {
        for (i = first; i < argc; i++) err |= convert(dic, argv[i], mode);
    } else {
        char line[65536];
        while (fgets(line, sizeof line, stdin)) {
            size_t n = strlen(line);
            while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
            err |= convert(dic, line, mode);
        }
    }
    cy_close(dic);
    return err;
}
