#!/usr/bin/env python3
"""ColorfulYomi の読みの一致率を測る。

    python tests/eval/eval.py --cyomi build/Release/cyomi.exe --dic colorful_talk.dic

sentences.tsv の各文を cyomi で記号列に変換し、アクセント記号・区切りを除いた読みを正解と比べる。
長音の書き方の違い（とうきょう / とーきょー、せんせい / せんせー）や、づ / ず・ぢ / じ は同じとみなす。
"""
import argparse
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

VOWEL = {}
for row, v in (("あかさたなはまやらわがざだばぱぁゃゎ", "あ"), ("いきしちにひみりぎじぢびぴぃ", "い"),
               ("うくすつぬふむゆるぐずづぶぷぅゅゔ", "う"), ("えけせてねへめれげぜでべぺぇ", "え"),
               ("おこそとのほもよろをごぞどぼぽぉょ", "お")):
    for ch in row:
        VOWEL[ch] = v


def kata2hira(s):
    return "".join(chr(ord(c) - 0x60) if "ァ" <= c <= "ヶ" else c for c in s)


def normalize(s):
    s = kata2hira(s)
    s = "".join(c for c in s if "ぁ" <= c <= "ゖ" or c == "ー")
    s = s.replace("を", "お").replace("づ", "ず").replace("ぢ", "じ")
    out = []
    for c in s:
        if c == "ー" and out:
            c = VOWEL.get(out[-1], "")
        out.append(c)
    s = "".join(out)
    # 長音の綴りの揺れ: おう→おお、えい→ええ
    res = []
    for c in s:
        if res and c == "う" and VOWEL.get(res[-1]) == "お":
            c = "お"
        elif res and c == "い" and VOWEL.get(res[-1]) == "え":
            c = "え"
        res.append(c)
    return "".join(res)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cyomi", required=True)
    ap.add_argument("--dic", required=True)
    ap.add_argument("--file", default=os.path.join(HERE, "sentences.tsv"))
    ap.add_argument("-v", "--verbose", action="store_true")
    a = ap.parse_args()

    items = []
    with open(a.file, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            text, ref = line.split("\t")
            items.append((text, ref.split("|")))
    inp = "\n".join(t for t, _ in items) + "\n"
    r = subprocess.run([a.cyomi, "-d", a.dic], input=inp.encode("utf-8"), capture_output=True)
    if r.returncode != 0:
        sys.exit(r.stderr.decode("utf-8", "replace"))
    outs = r.stdout.decode("utf-8").split("\n")
    ok = 0
    bad = []
    for (text, refs), koe in zip(items, outs):
        got = normalize(koe)
        if any(got == normalize(x) for x in refs):
            ok += 1
            if a.verbose:
                print(f"OK  {text}\t{koe}")
        else:
            bad.append((text, koe, refs[0]))
    for text, koe, ref in bad:
        print(f"NG  {text}\n    出力: {koe}\n    正解: {ref}")
    n = len(items)
    print(f"読みの一致: {ok}/{n} 文（{100.0 * ok / n:.1f}%）")


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    main()
