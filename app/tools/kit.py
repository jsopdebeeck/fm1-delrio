#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""The kit: the instruments' strokes, from WAV files to the C file the firmware plays (dsp/kit_samples.c, 16-bit mono,
each at its own rate).

  kit.py build [DIR] [--out dsp/kit_samples.c]   DIR (default app/kit): one WAV a stroke, named INSTRUMENT_STROKE.wav
                                                 (djembe_bass.wav, triangle_muted.wav...); a stroke may have several
                                                 takes, played in turn: maracas_shake_1.wav, maracas_shake_2.wav
  kit.py vcsl VCSL_DIR [DIR]                     the recorded strokes from VCSL (Versilian Community Sample Library,
                                                 CC0: github.com/sgossner/VCSL), into DIR as kit WAVs
  kit.py take FILE.wav NAME [--start S] [--end S] [--pitch ST] [--damp S] [DIR]
                                                 a stroke from any WAV (from S to S seconds into it: one stroke of a
                                                 recording that has several) into DIR as NAME.wav; --pitch raises it
                                                 ST semitones (a stone pressed on a wire), --damp stops it sooner
                                                 (60 dB down in S seconds)

Each stroke is cut from its start (the first sound) to its end (where it falls 60 dB, or the stroke's longest),
faded out, resampled to its instrument's rate and normalised (the takes of one stroke keep their levels between
them). The firmware's flash holds about 470 KB of them: the build prints the total. Any WAV works (mono or stereo,
16 or 24-bit, any rate): your own recordings replace the kit's, one file at a time. Needs numpy."""
import argparse
import re
import shutil
import sys
import wave
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent.parent
# the instruments in their order (app.c's, the black keys' left to right): their strokes, rate and each stroke's
# longest (s). The drums at 22050 Hz (little above 8 kHz in them), the bright ones at 44100
KIT = [
    ("djembe", 22050, [("bass", 0.50), ("tone", 0.40), ("slap", 0.30)]),
    ("cajon", 22050, [("bass", 0.55), ("slap", 0.45)]),
    ("udu", 22050, [("bass", 0.80), ("tone", 0.40), ("slap", 0.32)]),
    ("clapsticks", 44100, [("hit", 0.30)]),
    ("maracas", 44100, [("shake", 0.25)]),
    ("triangle", 44100, [("open", 1.60), ("muted", 0.35)]),
    ("berimbau", 16000, [("dom", 0.90), ("dim", 0.55), ("tsch", 0.20)]),   # the berimbau mode's (FX)
    ("caxixi", 32000, [("shake", 0.16)]),
]
# each stroke's level against its instrument's others, dB (normalised to the same peak, a short stroke sounds softer)
TRIM = {"djembe_tone": 1.5, "djembe_slap": 6.0, "cajon_bass": 3.0, "cajon_slap": 5.0, "udu_bass": -1.5,
        "udu_tone": 2.0, "udu_slap": 3.0, "berimbau_dom": 6.0, "berimbau_dim": 7.0, "berimbau_tsch": 8.0,
        "caxixi_shake": 6.0}
VCSL = {                                   # kit name: VCSL file (in its folder, any depth)
    "cajon_bass": "Cajon_hit3_f_rr1.wav",
    "cajon_slap": "Cajon_hit1_f_rr1.wav",
    "clapsticks_hit": "Claves1_Hit_v2_rr1_Mid.wav",
    "maracas_shake_1": "LShaker_Hit_rr1_Mid.wav",     # the large shaker's short strokes (its shakes are slow
    "maracas_shake_2": "LShaker_Hit_rr2_Mid.wav",     # swishes, their accent ~0.1 s late)
    "triangle_open": "Triangle3_Hit_v2_rr1_Mid.wav",
    "triangle_muted": "Triangle3_HitM_v2_rr1_Mid.wav",
}


def load(path):
    """mono float, its rate"""
    with wave.open(str(path)) as w:
        n, ch, sw, sr = w.getnframes(), w.getnchannels(), w.getsampwidth(), w.getframerate()
        raw = w.readframes(n)
    if sw == 2:
        x = np.frombuffer(raw, "<i2").astype(np.float64) / 32768.0
    elif sw == 3:
        b = np.frombuffer(raw, np.uint8).reshape(-1, 3).astype(np.int32)
        x = ((b[:, 0] | b[:, 1] << 8 | b[:, 2] << 16) << 8 >> 8).astype(np.float64) / 8388608.0
    elif sw == 4:
        x = np.frombuffer(raw, "<i4").astype(np.float64) / 2147483648.0
    else:
        raise SystemExit(f"{path}: {8 * sw}-bit samples (16, 24 or 32 only)")
    return x.reshape(-1, ch).mean(axis=1), sr


def resample(x, sr, rate):
    if sr == rate:
        return x
    pad = np.concatenate([x, np.zeros(len(x) // 4 + 256)])          # (no wrap of the end onto the start)
    n = int(round(len(pad) * rate / sr))
    sp = np.fft.rfft(pad)
    m = n // 2 + 1
    sp = sp[:m] if m <= len(sp) else np.concatenate([sp, np.zeros(m - len(sp))])
    y = np.fft.irfft(sp, n) * (n / len(pad))
    return y[:int(round(len(x) * rate / sr))]


def cut(x, sr, longest):
    """from the stroke's start to its end, faded"""
    a = np.abs(x)
    pk = a.max()
    if pk <= 0:
        raise SystemExit("a silent file")
    start = max(0, int(np.argmax(a > pk * 0.02)) - int(0.001 * sr))
    env = np.convolve(a, np.ones(256) / 256, "same")
    end = len(x) - int(np.argmax(env[::-1] > pk * 1e-3))           # -60 dB
    end = min(end, start + int(longest * sr))
    y = x[start:end].copy()
    natural = end - start < int(longest * sr)
    fade = int(0.02 * sr) if natural else min(len(y) * 2 // 5, int(0.5 * sr))   # cut short: a longer fade
    y[-fade:] *= 0.5 + 0.5 * np.cos(np.linspace(0, np.pi, fade))
    return y


def build(src, out):
    files = {p.stem: p for p in src.glob("*.wav")}
    lines, table, slots = [], [], []
    total = 0
    credits = (src / "CREDITS.txt").read_text().strip().splitlines() if (src / "CREDITS.txt").exists() else []
    for inst, rate, strokes in KIT:
        row = []
        for stroke, longest in strokes:
            name = f"{inst}_{stroke}"
            takes = sorted((k for k in files if re.fullmatch(re.escape(name) + r"(_\d+)?", k)),
                           key=lambda k: int(k.rsplit("_", 1)[1]) if k != name else 0)
            if not takes:
                raise SystemExit(f"kit: no {name}.wav in {src}")
            ys = []
            for k in takes:
                x, sr = load(files[k])
                ys.append(resample(cut(x, sr, longest), sr, rate))
            peak = max(np.abs(y).max() for y in ys)
            row.append((len(table), len(takes)))
            for k, y in zip(takes, ys):
                q = np.clip(np.round(y / peak * 32000), -32767, 32767).astype(np.int16)
                i = len(table)
                body = ",\n".join("    " + ", ".join(str(v) for v in q[j:j + 16]) for j in range(0, len(q), 16))
                lines.append(f"static const int16_t S{i}[{len(q)}] = {{   /* {k}: {len(q) / rate:.2f} s at {rate} Hz */\n"
                             f"{body}\n}};\n")
                table.append(f"    {{S{i}, {len(q)}u, {rate}.0f, {10 ** (TRIM.get(name, 0.0) / 20):.3f}f}},   /* {k} */")
                total += 2 * len(q)
        slots.append(row)
    nart = max(len(s) for _, _, s in KIT)
    first = ",\n".join("    {" + ", ".join(str(row[a][0]) if a < len(row) else "0" for a in range(nart)) + "}"
                       for row in slots)
    count = ",\n".join("    {" + ", ".join(str(row[a][1]) if a < len(row) else "0" for a in range(nart)) + "}"
                       for row in slots)
    head = ["/* SPDX-License-Identifier: MPL-2.0 (this file's code); the sounds: see below */",
            "/* generated by tools/kit.py from the kit's WAVs (app/kit): the strokes the voice plays (dsp/drums.h) */"]
    head += [f"/* {c} */" for c in credits]
    out.write_text("\n".join(head) + "\n#include \"drums.h\"\n\n" + "\n".join(lines) +
                   f"\nconst dr_sample_t DR_SAMPLES[{len(table)}] = {{\n" + "\n".join(table) + "\n};\n" +
                   f"const uint8_t DR_FIRST[DR_NINST][DR_NART] = {{\n{first}\n}};\n" +
                   f"const uint8_t DR_TAKES[DR_NINST][DR_NART] = {{\n{count}\n}};\n")
    print(f"kit: {len(table)} samples, {total // 1024} KB of flash -> {out}")


def save(path, x, sr):
    """mono, 16-bit, 3 s at most: a kit WAV"""
    y = x[:int(3.0 * sr)]
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes(np.clip(np.round(y * 32768), -32768, 32767).astype("<i2").tobytes())


def take(src, name, start, end, dst, pitch=0.0, damp=0.0):
    x, sr = load(src)
    x = x[int(start * sr):int(end * sr) if end else len(x)]
    if pitch:                                                       # higher (and shorter): played faster
        x = resample(x, sr, int(round(sr / 2 ** (pitch / 12))))
    if damp:
        x = x * np.exp(-6.9078 * np.arange(len(x)) / (damp * sr))
    dst.mkdir(parents=True, exist_ok=True)
    save(dst / f"{name}.wav", x, sr)
    print(f"{dst / name}.wav  <- {src} {start:g}..{end or len(x) / sr + start:g} s")


def vcsl(vdir, dst):
    found = {p.name: p for p in Path(vdir).rglob("*.wav")}
    dst.mkdir(parents=True, exist_ok=True)
    for name, f in VCSL.items():
        if f not in found:
            raise SystemExit(f"kit: {f} is not in {vdir}")
        x, sr = load(found[f])
        save(dst / f"{name}.wav", x, sr)
        print(f"{dst / name}.wav  <- {f}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    b = sub.add_parser("build")
    b.add_argument("dir", type=Path, nargs="?", default=HERE / "kit")
    b.add_argument("--out", type=Path, default=HERE / "dsp" / "kit_samples.c")
    v = sub.add_parser("vcsl")
    v.add_argument("vcsl_dir", type=Path)
    v.add_argument("dir", type=Path, nargs="?", default=HERE / "kit")
    t = sub.add_parser("take")
    t.add_argument("file", type=Path)
    t.add_argument("name")
    t.add_argument("--start", type=float, default=0.0)
    t.add_argument("--end", type=float, default=0.0)
    t.add_argument("--pitch", type=float, default=0.0)
    t.add_argument("--damp", type=float, default=0.0)
    t.add_argument("dir", type=Path, nargs="?", default=HERE / "kit")
    a = ap.parse_args()
    if a.cmd == "build":
        build(a.dir, a.out)
    elif a.cmd == "vcsl":
        vcsl(a.vcsl_dir, a.dir)
    else:
        take(a.file, a.name, a.start, a.end, a.dir, a.pitch, a.damp)


if __name__ == "__main__":
    main()
