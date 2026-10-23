/* SPDX-License-Identifier: MPL-2.0 */
/* The kit's voice (drums.h). Sixteen voices, each a stroke read at its rate times the instrument's tuning and its
 * glide (linear interpolation), with an envelope that only ever falls: 1 while the stroke rings as recorded, or an
 * exponential fall once the instrument is shortened, or a fast one when the stroke is stopped (the triangle muted, a
 * voice taken for a newer stroke). */
#include "drums.h"
#include "fastmath.h"

#define NV 16u                           /* voices */
#define PER_INST 4u                      /* an instrument's strokes sounding at once, at most */
#define HOLD_S 0.012f                    /* a shortened stroke keeps its attack this long before it falls */

typedef struct {
    const int16_t *pcm;
    uint32_t len, delay, age, hold;
    float pos, inc, g, env, fall, wires, lp;   /* lp: the split for the wires (below ~1.8 kHz) */
    uint8_t on, inst, art, stopping;
} voice_t;

static voice_t V[NV];
static dr_inst_t P[DR_NINST];
static float SR = 44100.0f, stop_fall, split_k;
static uint32_t age_now;
static uint8_t take_next[DR_NINST][DR_NART];
static volatile float bend_to[DR_NINST];   /* the glide: where it goes (main loop), where it is (audio) */
static volatile uint8_t bend_art[DR_NINST];
static float bend_now[DR_NINST];

static struct {                          /* the click: a short sine */
    float ph, inc, env, amp;
    uint32_t delay;
} clk;

void dr_init(float sample_rate)
{
    uint32_t i;
    SR = sample_rate;
    stop_fall = fm_expf(-1.0f / (0.004f * SR));   /* stopped: gone in a few ms */
    split_k = 1.0f - fm_expf(-6.2831853f * 1800.0f / SR);   /* a one-pole split at 1.8 kHz */
    for (i = 0; i < DR_NINST; i++) {
        P[i].level = 0.8f;
        P[i].tune = 0.0f;
        P[i].decay = 1.0f;
        P[i].wires = 1.0f;
        bend_to[i] = bend_now[i] = 0.0f;
        bend_art[i] = 0;
    }
    dr_panic();
}

void dr_inst(uint32_t inst, const dr_inst_t *p)
{
    if (inst < DR_NINST)
        P[inst] = *p;
}

void dr_bend(uint32_t inst, uint32_t art, float semis)
{
    if (inst >= DR_NINST)
        return;
    bend_art[inst] = (uint8_t)art;
    bend_to[inst] = semis;
}

float dr_bent(uint32_t inst) { return inst < DR_NINST ? bend_now[inst] : 0.0f; }

void dr_panic(void)
{
    uint32_t i;
    for (i = 0; i < NV; i++)
        V[i].on = 0;
    clk.env = 0.0f;
}

static void stop(voice_t *v)
{
    v->stopping = 1;
    v->hold = 0;
    v->fall = stop_fall;
}

void dr_hit(uint32_t inst, uint32_t art, float vel, uint32_t delay)
{
    const dr_inst_t *p;
    const dr_sample_t *s;
    voice_t *v = 0;
    uint32_t i, a, n, mine = 0, oldest = 0, take;
    float g, tau;
    if (inst >= DR_NINST || art < 1u || art > DR_NART || !(n = DR_TAKES[inst][art - 1u]))
        return;
    a = art - 1u;
    p = &P[inst];
    take = take_next[inst][a] % n;       /* the takes in turn: a shaker's down and up */
    take_next[inst][a] = (uint8_t)((take + 1u) % n);
    s = &DR_SAMPLES[DR_FIRST[inst][a] + take];
    if ((inst == DR_TRIANGLE && art == 2u) || inst == DR_MARACAS)   /* the hand on the triangle; the maracas' next stroke */
        for (i = 0; i < NV; i++)
            if (V[i].on && V[i].inst == inst && !V[i].stopping)
                stop(&V[i]);
    /* the voice: this instrument's oldest when it has PER_INST sounding, else a free one, else the oldest of all */
    for (i = 0; i < NV; i++)
        if (V[i].on && V[i].inst == inst && !V[i].stopping)
            mine++;
    if (mine < PER_INST)
        for (i = 0; i < NV && !v; i++)
            if (!V[i].on)
                v = &V[i];
    if (!v)
        for (i = 0; i < NV; i++)
            if (V[i].on && (mine < PER_INST || V[i].inst == inst) && age_now - V[i].age >= oldest) {
                oldest = age_now - V[i].age;
                v = &V[i];
            }
    if (!v)
        return;
    g = fm_clampf(vel, 0.0f, 1.0f);
    g = g * fm_sqrtf(g) * p->level * s->gain;   /* the velocity's curve: soft strokes softer */
    v->g = g;
    v->wires = fm_clampf(p->wires, 0.0f, 1.0f);
    v->lp = 0.0f;
    v->pcm = s->pcm;
    v->len = s->len;
    v->pos = 0.0f;
    v->inc = s->rate / SR * fm_exp2f((fm_clampf(p->tune, -12.0f, 12.0f) +
                                      (art == bend_art[inst] ? bend_now[inst] : 0.0f)) * (1.0f / 12.0f));
    v->env = 1.0f;
    if (p->decay >= 0.995f) {
        v->fall = 1.0f;
        v->hold = 0;
    } else {                             /* the ring's time constant: 20 ms .. 1.2 s */
        float d = fm_clampf(p->decay, 0.1f, 1.0f);
        tau = 0.02f + d * d * 1.2f;
        v->fall = fm_expf(-1.0f / (tau * SR));
        v->hold = (uint32_t)(HOLD_S * SR);
    }
    v->delay = delay;
    v->age = ++age_now;
    v->inst = (uint8_t)inst;
    v->art = (uint8_t)art;
    v->stopping = 0;
    v->on = 1;
}

void dr_choke(uint32_t inst)
{
    uint32_t i;
    for (i = 0; i < NV; i++)
        if (V[i].on && V[i].inst == inst && !V[i].stopping)
            stop(&V[i]);
}

void dr_click(int accent, uint32_t delay)
{
    clk.ph = 0.0f;
    clk.inc = (accent ? 2093.0f : 1568.0f) / SR;   /* C7, G6 */
    clk.amp = accent ? 0.32f : 0.2f;
    clk.env = 1.0f;
    clk.delay = delay;
}

uint32_t dr_active(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < NV; i++)
        n += V[i].on;
    return n;
}

void dr_render(float *out, uint32_t n)
{
    uint32_t i, k;
    const float cfall = fm_expf(-1.0f / (0.012f * SR));
    float glide[DR_NINST];
    for (i = 0; i < n; i++)
        out[i] = 0.0f;
    for (k = 0; k < DR_NINST; k++) {     /* the glides, a block at a time: there in about 30 ms */
        float was = bend_now[k];
        bend_now[k] += (bend_to[k] - bend_now[k]) * 0.18f;
        glide[k] = fm_exp2f((bend_now[k] - was) * (1.0f / 12.0f));
    }
    for (k = 0; k < NV; k++) {
        voice_t *v = &V[k];
        if (!v->on)
            continue;
        if (v->art == bend_art[v->inst] && glide[v->inst] != 1.0f)
            v->inc *= glide[v->inst];
        for (i = 0; i < n; i++) {
            uint32_t j;
            float s, f;
            if (v->delay) {
                v->delay--;
                continue;
            }
            j = (uint32_t)v->pos;
            if (j + 1u >= v->len || v->env < 0.0003f) {
                v->on = 0;
                break;
            }
            f = v->pos - (float)j;
            s = ((float)v->pcm[j] + (float)(v->pcm[j + 1u] - v->pcm[j]) * f) * (1.0f / 32768.0f) * v->env;
            if (v->wires < 0.999f) {     /* less of what is above the split: the wires slackened */
                v->lp += (s - v->lp) * split_k;
                s = v->lp + (s - v->lp) * v->wires;
            }
            out[i] += s * v->g;
            v->pos += v->inc;
            if (v->hold)
                v->hold--;
            else
                v->env *= v->fall;
        }
    }
    if (clk.env > 0.0003f)
        for (i = 0; i < n; i++) {
            float s;
            if (clk.delay) {
                clk.delay--;
                continue;
            }
            s = fm_sin_turns(clk.ph) * clk.amp * clk.env;
            clk.ph += clk.inc;
            if (clk.ph >= 1.0f)
                clk.ph -= 1.0f;
            clk.env *= cfall;
            out[i] += s;
        }
}
