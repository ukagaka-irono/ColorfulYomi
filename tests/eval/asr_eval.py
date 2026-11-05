#!/usr/bin/env python3
"""文 → ColorfulYomi → ColorfulTalk V3 → Whisper で、文全体が伝わるかを測る（任意の試験）。

    python tests/eval/asr_eval.py --cyomi build/Release/cyomi.exe --dic colorful_talk.dic \
        --ctalk ../ColorfulTalk-V3/build/Release/ctalk.exe --out OUTDIR

必要: openai-whisper（small）、pykakasi。正解は sentences.tsv の読み、Whisper の書き起こしは pykakasi で読みにして、
ローマ字の文字誤り率（CER）を出す。
"""
import argparse
import json
import os
import re
import subprocess
import sys

import pykakasi
import whisper

HERE = os.path.dirname(os.path.abspath(__file__))
kks = pykakasi.kakasi()


def norm(s):
    h = "".join(x["hira"] for x in kks.convert(s))
    h = re.sub(r"[^ぁ-ゟー]", "", h)
    r = "".join(x["hepburn"] for x in kks.convert(h))
    r = r.replace("ou", "o").replace("ei", "e")
    r = re.sub(r"([aiueo])\1+", r"\1", r)
    r = r.replace("wo", "o").replace("di", "ji").replace("du", "zu")
    r = re.sub(r"(?<![sc])ha", "wa", r)
    r = re.sub(r"(?<![a-z])he", "e", r)
    return r


def cer(ref, hyp):
    d = list(range(len(hyp) + 1))
    for i, a in enumerate(ref, 1):
        prev, d[0] = d[0], i
        for j, b in enumerate(hyp, 1):
            prev, d[j] = d[j], min(d[j] + 1, d[j - 1] + 1, prev + (a != b))
    return d[len(hyp)] / max(1, len(ref))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cyomi", required=True)
    ap.add_argument("--dic", required=True)
    ap.add_argument("--ctalk", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    items = []
    with open(os.path.join(HERE, "sentences.tsv"), encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if line and not line.startswith("#"):
                text, ref = line.split("\t")
                items.append((text, ref.split("|")[0]))
    r = subprocess.run([a.cyomi, "-d", a.dic], input=("\n".join(t for t, _ in items) + "\n").encode("utf-8"),
                       capture_output=True, check=True)
    koes = r.stdout.decode("utf-8").split("\n")
    model = whisper.load_model("small")
    res = []
    for i, ((text, ref), koe) in enumerate(zip(items, koes)):
        kp = os.path.join(a.out, f"s{i:03d}.txt")
        wp = os.path.join(a.out, f"s{i:03d}.wav")
        with open(kp, "w", encoding="utf-8") as f:
            f.write(koe)
        subprocess.run([a.ctalk, "-f", kp, "-o", wp], check=True, capture_output=True)
        hyp = model.transcribe(wp, language="ja", fp16=False, temperature=0.0)["text"]
        c = cer(norm(ref), norm(hyp))
        res.append({"text": text, "koe": koe, "hyp": hyp, "cer": c})
        print(f"{i:3d} CER {100 * c:5.1f}%  {text} → {hyp}", flush=True)
    with open(os.path.join(a.out, "asr.json"), "w", encoding="utf-8") as f:
        json.dump(res, f, ensure_ascii=False, indent=1)
    n = len(res)
    print(f"平均 CER {100 * sum(r['cer'] for r in res) / n:.1f}%、"
          f"CER 0 の文 {sum(r['cer'] == 0 for r in res)}/{n}、CER 10% 以下 {sum(r['cer'] <= 0.1 for r in res)}/{n}")


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    main()
