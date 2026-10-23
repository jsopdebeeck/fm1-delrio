#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""The djembe's and the udu's strokes, synthesised: each a few decaying partials, as a drum head or a clay pot rings,
over a short noise of the hand. For the strokes the kit has no recording of: writes 16-bit mono WAVs at 44100 Hz into
the kit's folder (tools/kit.py makes the firmware's samples from them).

  synth.py [NAME ...] [--out app/kit]     the strokes named, or else those the kit's folder has no WAV for

  djembe_bass   the shell's air (Helmholtz, ~75 Hz) and the head's low mode: the palm flat in the centre
  djembe_tone   the head's modes from ~370 Hz: fingers together at the edge
  djembe_slap   the high modes and a crack of noise: fingers loose at the edge
  udu_bass      the pot's air through the side hole, its pitch rising as the palm leaves the hole
  udu_tone      the clay's modes: fingers on the belly
  udu_slap      a short pop on the top opening
Needs numpy."""
import argparse
import wave
from pathlib import Path

import numpy as np

SR = 44100
rng = np.random.default_rng(1)            # the same strokes at every run


def t_of(dur):
    return np.arange(int(dur * SR)) / SR


def partial(dur, f0, f1, glide, amp, tau, t0=0.0):
    """a decaying sine whose frequency goes from f0 to f1 (exponentially, time constant glide), from t0"""
    t = t_of(dur)
    f = f1 + (f0 - f1) * np.exp(-t / glide) if glide > 0 else np.full_like(t, f0)
    ph = 2 * np.pi * np.cumsum(f) / SR
    y = amp * np.sin(ph) * np.exp(-t / tau)
    if t0:
        y = np.concatenate([np.zeros(int(t0 * SR)), y])[:len(t)]
    return y


def noise(dur, lo, hi, amp, tau):
    """noise between lo and hi Hz (FFT band-pass), decaying"""
    n = int(dur * SR)
    sp = np.fft.rfft(rng.standard_normal(n))
    f = np.fft.rfftfreq(n, 1 / SR)
    sp[(f < lo) | (f > hi)] = 0
    y = np.fft.irfft(sp, n)
    y /= np.abs(y).max() + 1e-12
    return amp * y * np.exp(-t_of(dur) / tau)


def attack(y, ms):
    """a soft start (a half cosine over ms): no click"""
    n = int(ms * SR / 1000)
    y[:n] *= 0.5 - 0.5 * np.cos(np.linspace(0, np.pi, n))
    return y


def mix(*parts):
    n = max(len(p) for p in parts)
    y = np.zeros(n)
    for p in parts:
        y[:len(p)] += p
    return y


def djembe_bass():
    d = 0.6
    return attack(mix(partial(d, 92, 74, 0.05, 1.0, 0.17),          # the shell's air
                      partial(d, 230, 205, 0.03, 0.35, 0.05),       # the head's lowest mode
                      partial(d, 148, 140, 0.03, 0.2, 0.08),
                      noise(d, 60, 900, 0.25, 0.012)), 1.5)        # the palm


def djembe_tone():
    d = 0.45
    f = 372
    modes = [(1.0, 1.0, 0.13), (1.59, 0.45, 0.08), (2.14, 0.32, 0.06), (2.30, 0.22, 0.05), (2.65, 0.14, 0.04),
             (2.92, 0.08, 0.03)]                                    # a circular head's modes: ratio, level, decay
    return attack(mix(*[partial(d, f * r * 1.025, f * r, 0.03, a, tau) for r, a, tau in modes],
                      partial(d, 80, 76, 0.05, 0.12, 0.1),          # a little of the shell's air
                      noise(d, 1500, 6000, 0.12, 0.006)), 0.8)


def djembe_slap():
    d = 0.32
    f = 390
    modes = [(1.0, 0.45, 0.06), (2.30, 0.5, 0.045), (3.5, 0.5, 0.035), (4.6, 0.4, 0.03), (6.1, 0.3, 0.025),
             (7.4, 0.22, 0.02)]
    return attack(mix(*[partial(d, f * r * 1.03, f * r, 0.015, a, tau) for r, a, tau in modes],
                      noise(d, 1200, 9000, 0.9, 0.018),             # the crack of the fingers
                      noise(d, 3000, 12000, 0.35, 0.006)), 0.3)


def udu_bass():
    d = 0.9
    return attack(mix(partial(d, 68, 112, 0.09, 1.0, 0.28),         # the air in the pot, the hole opening
                      partial(d, 136, 224, 0.09, 0.14, 0.18),
                      noise(d, 40, 400, 0.18, 0.03)), 6.0)          # the puff through the hole


def udu_tone():
    d = 0.45
    modes = [(430, 1.0, 0.09), (775, 0.55, 0.06), (1160, 0.35, 0.045), (1690, 0.22, 0.03), (2380, 0.12, 0.02)]
    return attack(mix(*[partial(d, f * 1.01, f, 0.02, a, tau) for f, a, tau in modes],
                      partial(d, 108, 108, 0, 0.25, 0.12),          # the air answers a little
                      noise(d, 800, 5000, 0.18, 0.005)), 0.6)


def udu_slap():
    d = 0.35
    return attack(mix(partial(d, 165, 230, 0.03, 1.0, 0.07),        # the top opening's air, short
                      partial(d, 520, 500, 0.02, 0.35, 0.03),
                      noise(d, 300, 4000, 0.45, 0.008)), 0.5)


STROKES = {"djembe_bass": djembe_bass, "djembe_tone": djembe_tone, "djembe_slap": djembe_slap,
           "udu_bass": udu_bass, "udu_tone": udu_tone, "udu_slap": udu_slap}


def write(path, y):
    y = y / (np.abs(y).max() + 1e-12) * 0.89                       # -1 dBFS
    fade = int(0.03 * SR)
    y[-fade:] *= np.linspace(1, 0, fade)
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes((y * 32767).astype("<i2").tobytes())


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("names", nargs="*", choices=[[]] + list(STROKES), metavar="NAME")
    ap.add_argument("--out", type=Path, default=Path(__file__).resolve().parent.parent / "kit")
    a = ap.parse_args()
    a.out.mkdir(parents=True, exist_ok=True)
    for name in a.names or [n for n in STROKES if not (a.out / f"{n}.wav").exists()]:
        write(a.out / f"{name}.wav", STROKES[name]())
        print(f"{a.out / name}.wav")


if __name__ == "__main__":
    main()
