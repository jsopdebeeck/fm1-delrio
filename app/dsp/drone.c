/* SPDX-License-Identifier: MPL-2.0 */
/* The drones (drone.h).
 *
 * The tanpura: four strings, tuned first string (Pa, Ma or Ni, under Sa), Sa, Sa, and Sa an octave down, plucked in
 * that order, evenly, with a longer pause before the cycle starts again. A string: a tone rich in harmonics (a
 * band-limited saw and a narrow pulse) through two paths, a warm body (a low-pass) and the jawari: a resonance that,
 * from each pluck, sweeps down through the harmonics as the string rings (the flat bridge the string grazes), each
 * pluck a little differently. A finger's soft start, a long ring (about 10 s, the strings overlapping into one
 * sound), each string a few cents off true, as a real one is.
 *
 * The shruti box: a reed for each note held, a band-limited saw and a narrow pulse (polyBLEP) mixed, softened by two
 * one-pole low-passes; each note fades in and out; the whole breathes with the bellows: each stroke of them a swell
 * and an ebb of the air (louder and a little brighter as it presses), each a little longer or shorter than the last. */
#include "drone.h"
#include "fastmath.h"

#define NS 4u
#define SHN 27u                          /* the shruti's notes: the FM-1's keys */

typedef struct {
    float ph, inc;                       /* the tone */
    float env, fall;                     /* the ring */
    uint8_t rising;                      /* the finger's start: env climbing */
    float sweep, sweep_fall;             /* the jawari's resonance: 1 high .. 0 low */
    float fhi, flo;                      /* its range, Hz */
    float body, lo, band;                /* the body's low-pass; the resonance (state-variable filter) */
} tstring_t;

static tstring_t S[NS];
static float SR = 44100.0f, rise_k, body_k;
static uint32_t rnd = 0x9E3779B9u;

static struct {
    volatile int on;
    volatile int32_t sa;
    volatile uint8_t first;
    volatile float cycle_s, level;
    float next;                          /* samples to the next pluck */
    uint32_t which;                      /* the string plucked next */
    volatile int last;
    float gain;                          /* glides to on ? level : 0 */
} tp;

static struct {
    volatile int on;
    volatile uint32_t notes;
    volatile int32_t low;
    volatile float level, bellows;
    float ph[SHN], amp[SHN];
    float lp1, lp2, breath, breath_s, gain;
} sh;

static float noise(void)
{
    rnd = rnd * 1664525u + 1013904223u;
    return (float)(int32_t)rnd * (1.0f / 2147483648.0f);
}

static float hz(float midi) { return 440.0f * fm_exp2f((midi - 69.0f) * (1.0f / 12.0f)); }

void dn_init(float sample_rate)
{
    SR = sample_rate;
    rise_k = 1.0f - fm_expf(-1.0f / (0.004f * SR));       /* the finger: about 10 ms to full */
    body_k = 1.0f - fm_expf(-6.2831853f * 900.0f / SR);
    tp.cycle_s = 4.0f;
    tp.level = 0.7f;
    tp.sa = 48;
    tp.last = -1;
    sh.low = 53;
    sh.level = 0.7f;
    dn_panic();
}

void dn_panic(void)
{
    uint32_t i;
    tp.on = 0;
    sh.on = 0;
    tp.gain = sh.gain = 0.0f;
    for (i = 0; i < NS; i++) {
        S[i].env = 0.0f;
        S[i].rising = 0;
        S[i].body = S[i].lo = S[i].band = 0.0f;
    }
    for (i = 0; i < SHN; i++)
        sh.amp[i] = 0.0f;
}

void dn_tanpura(int on, const dn_tanpura_t *p)
{
    if (on && !tp.on) {                  /* from the first string, at once */
        tp.next = 0.0f;
        tp.which = 0;
    }
    tp.sa = p->sa;
    tp.first = p->first;
    tp.cycle_s = fm_clampf(p->cycle_s, 1.0f, 12.0f);
    tp.level = fm_clampf(p->level, 0.0f, 1.0f);
    tp.on = on;
}

void dn_shruti(int on, uint32_t notes, int32_t low, float level, float bellows)
{
    sh.notes = notes;
    sh.low = low;
    sh.level = fm_clampf(level, 0.0f, 1.0f);
    sh.bellows = fm_clampf(bellows, 0.0f, 1.0f);
    sh.on = on;
}

int dn_string(void) { return tp.last; }

/* string i plucked: its pitch (a few cents off), its ring, its jawari sweeping from the top again */
static void pluck(uint32_t i)
{
    static const int8_t FIRST[3] = {-5, -7, -1};   /* Pa, Ma, Ni under Sa */
    tstring_t *s = &S[i];
    float midi = (float)tp.sa + (i == 0u ? (float)FIRST[tp.first % 3u] : i == 3u ? -12.0f : 0.0f) + 0.04f * noise();
    float f = hz(midi), ring = i == 3u ? 12.0f : 10.0f;
    s->inc = f / SR;
    s->fall = fm_expf(-6.9077553f / (ring * SR));   /* -60 dB in 10 s (the low Sa 12) */
    s->rising = 1;
    s->sweep = 1.0f;
    s->sweep_fall = 2.2f * (0.85f + 0.3f * (0.5f + 0.5f * noise()));   /* seconds: each pluck a little different */
    s->fhi = fm_clampf(30.0f * f, 1800.0f, 5200.0f);
    s->flo = fm_clampf(4.0f * f, 250.0f, 900.0f);
    tp.last = (int)i;
}

static float blep(float t, float dt);

static void tanpura_render(float *out, uint32_t n)
{
    static const float AT[NS + 1] = {0.0f, 1.0f, 2.0f, 3.0f, 4.6f};   /* the plucks in a cycle's units */
    float target = tp.on ? tp.level * 0.24f : 0.0f, unit = tp.cycle_s * SR / AT[NS], fco[NS];
    uint32_t i, k;
    if (target == 0.0f && tp.gain < 0.0001f) {
        tp.gain = 0.0f;
        return;
    }
    for (k = 0; k < NS; k++) {           /* the resonances, where their sweep is, once a block */
        tstring_t *s = &S[k];
        float sw = s->sweep * fm_sqrtf(s->sweep), fc = s->flo + (s->fhi - s->flo) * sw;
        fco[k] = 2.0f * fm_sinf(3.14159265f * fc / SR);
        s->sweep *= fm_expf(-(float)n / (s->sweep_fall * SR));
    }
    for (i = 0; i < n; i++) {
        float sum = 0.0f;
        if (tp.on) {
            if (tp.next <= 0.0f) {
                pluck(tp.which);
                tp.next += (AT[tp.which + 1u] - AT[tp.which]) * unit;
                tp.which = (tp.which + 1u) % NS;
            }
            tp.next -= 1.0f;
        }
        for (k = 0; k < NS; k++) {
            tstring_t *s = &S[k];
            float t = s->ph, t2, saw, src, hi;
            if (s->env < 0.00005f && !s->rising)
                continue;
            saw = 2.0f * t - 1.0f - blep(t, s->inc);
            t2 = t + 0.88f;                  /* a narrow pulse: the buzz's edge */
            if (t2 >= 1.0f)
                t2 -= 1.0f;
            src = 0.55f * saw + 0.6f * (saw - (2.0f * t2 - 1.0f - blep(t2, s->inc)));
            s->body += (src - s->body) * body_k;
            s->lo += fco[k] * s->band;       /* the jawari: a resonance (Q about 5) sweeping down */
            hi = src - s->lo - 0.18f * s->band;
            s->band += fco[k] * hi;
            if (s->rising) {
                s->env += (1.02f - s->env) * rise_k;
                if (s->env >= 1.0f)
                    s->rising = 0;
            } else
                s->env *= s->fall;
            sum += s->env * (0.6f * s->body + 0.9f * s->band);
            t += s->inc;
            s->ph = t >= 1.0f ? t - 1.0f : t;
        }
        tp.gain += (target - tp.gain) * 0.0005f;
        out[i] += sum * tp.gain;
    }
}

static float blep(float t, float dt)
{
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

static void shruti_render(float *out, uint32_t n)
{
    float inc[SHN], target = sh.on ? sh.level : 0.0f, norm;
    uint32_t i, k, notes = sh.notes, held = 0;
    if (target == 0.0f && sh.gain < 0.0001f) {
        sh.gain = 0.0f;
        return;
    }
    for (k = 0; k < SHN; k++) {
        inc[k] = hz((float)(sh.low + (int32_t)k)) / SR;
        held += notes >> k & 1u;
    }
    norm = 0.32f / fm_sqrtf((float)(held ? held : 1u));
    if (sh.breath_s <= 0.0f)
        sh.breath_s = 3.8f;
    for (i = 0; i < n; i++) {
        float sum = 0.0f, b, press;
        for (k = 0; k < SHN; k++) {
            float want = (float)(notes >> k & 1u), t, saw, pulse, t2;
            sh.amp[k] += (want - sh.amp[k]) * 0.0006f;   /* a note opens and closes in about 40 ms */
            if (sh.amp[k] < 0.0005f && !want)
                continue;
            t = sh.ph[k];
            saw = 2.0f * t - 1.0f - blep(t, inc[k]);
            t2 = t + 0.62f;                  /* a narrow pulse: a saw minus the same saw, shifted */
            if (t2 >= 1.0f)
                t2 -= 1.0f;
            pulse = saw - (2.0f * t2 - 1.0f - blep(t2, inc[k]));
            sum += (0.65f * saw + 0.35f * pulse) * sh.amp[k];
            t += inc[k];
            sh.ph[k] = t >= 1.0f ? t - 1.0f : t;
        }
        sh.breath += 1.0f / (sh.breath_s * SR);   /* the bellows: a swell and an ebb a stroke */
        if (sh.breath >= 1.0f) {
            sh.breath -= 1.0f;
            sh.breath_s = 3.8f * (0.85f + 0.3f * (0.5f + 0.5f * noise()));   /* the next a little longer or shorter */
        }
        press = 0.5f - 0.5f * fm_cosf(6.2831853f * sh.breath);   /* 0 .. 1 .. 0 */
        b = 1.0f - sh.bellows * 0.7f * (1.0f - press);
        sh.lp1 += (sum - sh.lp1) * (0.22f + 0.14f * sh.bellows * press);   /* pressed harder: a little brighter */
        sh.lp2 += (sh.lp1 - sh.lp2) * 0.42f;
        sh.gain += (target - sh.gain) * 0.00012f;   /* the bellows filling: about 200 ms */
        out[i] += sh.lp2 * norm * b * sh.gain;
    }
}

void dn_render(float *out, uint32_t n)
{
    tanpura_render(out, n);
    shruti_render(out, n);
}
