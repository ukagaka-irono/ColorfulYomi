# ColorfulYomi

漢字かな交じり文を [ColorfulTalk](https://github.com/ukagaka-irono/ColorfulTalk) の記号列に変換する、
無料（MIT ライセンス）の言語処理エンジンです。

```
今日はいい天気ですね。
  → きょ'ーわ/い'い/て'んきですね。        （V1 / V2 / V3 用のカナ記号列）
  → kyo'-wa/i'i/te'Nkidesune.               （nano / pico LSI 用のローマ字記号列）
```

- 約 48 万語の辞書（[Colorful-dictionary](https://github.com/ukagaka-irono/Colorful-dictionary)）で形態素解析し、読み・アクセント・句の区切りを決める
- 数（`1,280円` `12.5%` `090-1234-5678`）、日付（`11月5日`）、助数詞の音の変化（`3本` `6分` `1人`）、英字（`AI`）を読む
- ユーザー辞書（CSV）で読みとアクセントを追加できる
- C99、外部ライブラリなし。日常文 1,000 文を約 0.2 秒で変換
- 日常文 100 文で読みの一致率 97%（[docs/accuracy.md](docs/accuracy.md)）

## 使い方

```c
#include "colorful_yomi.h"
#include "colorful_talk_v3.h"

CyDic* dic;
char koe[1024];

cy_open("colorful_talk.dic", &dic);
if (cy_text_to_koe(dic, "今日はいい天気ですね。", koe, sizeof koe, NULL) == CT_OK) {
    /* koe を ct3_start(work, koe, &params) に渡して合成する */
}
cy_close(dic);
```

API・ユーザー辞書・処理の詳細は [docs/spec.md](docs/spec.md) を参照してください。

## ビルド

```bash
cmake -S . -B build
```

```bash
cmake --build build --config Release
```

`colorful_yomi`（静的ライブラリ）、`colorful_yomi`（DLL / 共有ライブラリ）、コマンド `cyomi` ができます。

辞書は [Colorful-dictionary](https://github.com/ukagaka-irono/Colorful-dictionary) の[リリース](https://github.com/ukagaka-irono/Colorful-dictionary/releases/tag/v1)から `colorful_talk.dic` を取得するか、
そちらの `tools/build_dic.py` で作ってください。

## コマンド

```bash
cyomi -d colorful_talk.dic "今日はいい天気ですね。"
```

| オプション | 内容 |
|---|---|
| `-d DIC` | 辞書（省略時は環境変数 `CY_DIC`、なければ cyomi と同じフォルダの `colorful_talk.dic`） |
| `-u CSV` | ユーザー辞書を追加（複数指定可） |
| `-r` | ローマ字記号列を出す |
| `-m` | 形態素解析の結果を出す（調べる用） |

文を指定しなければ、標準入力の 1 行ずつを変換します。V3 で読み上げる例:

```bash
cyomi "おはようございます。" > koe.txt
```

```bash
ctalk -f koe.txt -o hello.wav
```

## 試験

```bash
CY_DIC=colorful_talk.dic ctest --test-dir build -C Release
```

```bash
python tests/eval/eval.py --cyomi build/Release/cyomi.exe --dic colorful_talk.dic
```

## ライセンス

MIT License（[LICENSE](LICENSE)）。辞書ファイルは元データのライセンス（BSD 3-Clause）に従います。
辞書を同梱して配布するときは Colorful-dictionary の `NOTICE` も同梱してください。
