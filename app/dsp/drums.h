/* SPDX-License-Identifier: MPL-2.0 */
/* The kit's voice: recorded (or synthesised) strokes played back, each instrument at its level, tuned and shortened as
 * the app allows it; a stroke's pitch can glide while it rings (the udu's bass, the palm over the opening); the
 * triangle's muted stroke stops its ringing, the berimbau's and the maracas' next stroke the one before; a stroke with several takes plays them in turn. And the metronome's
 * click. The audio interrupt calls everything here but dr_inst and dr_bend (the main loop's).
 * Compiled apart at -O2 (fm1.toml o2): only fastmath.h, no libm, no doubles. */
#pragma once
#include <stdint.h>

#define DR_NINST 8u                      /* app.c's instruments, in its order: the kit's six, the berimbau, its caxixi */
#define DR_NART 3u                       /* strokes an instrument has at most */
#define DR_MARACAS 4u                    /* one pair of hands: a stroke stops the one before (short at a fast tempo) */
#define DR_TRIANGLE 5u                   /* its muted stroke (2) stops the open one ringing */
#define DR_BERIMBAU 6u                   /* one wire: a stroke stops the one before */

/* a stroke (tools/kit.py writes them, dsp/kit_samples.c): mono at its own rate, in blocks of DR_BLOCK samples of 8
 * bits, each block with its shift (a sample is pcm[i] << shift[i / DR_BLOCK]): half the flash of 16 bits, the noise
 * about 44 dB under each block's loudest, in the loud attack as in the quiet tail */
#define DR_BLOCK 16u
typedef struct {
    const int8_t *pcm;
    const uint8_t *shift;
    uint32_t len;
    float rate;
    float gain;                          /* its level against its instrument's other strokes */
} dr_sample_t;
extern const dr_sample_t DR_SAMPLES[];
extern const uint8_t DR_FIRST[DR_NINST][DR_NART];   /* stroke a (0..) of instrument i: its first sample */
extern const uint8_t DR_TAKES[DR_NINST][DR_NART];   /* and how many takes (0: no such stroke) */

typedef struct {
    float level;                         /* 0 .. 1 */
    float tune;                          /* semitones, -12 .. +12 */
    float decay;                         /* 0.1 .. 1: 1 rings as recorded, less shortens it */
    float wires;                         /* 0 .. 1: its sound above ~1.8 kHz (the cajon's snare wires), 1 as recorded */
} dr_inst_t;

void dr_init(float sample_rate);
void dr_inst(uint32_t inst, const dr_inst_t *p);   /* main loop: applies to the strokes that start after it */
/* main loop: the pitch of an instrument's stroke art (1..) while it rings, semitones (0 as played); it glides there */
void dr_bend(uint32_t inst, uint32_t art, float semis);
/* audio interrupt: a stroke (art 1..) at velocity 0..1, starting delay samples into the next render */
void dr_hit(uint32_t inst, uint32_t art, float vel, uint32_t delay);
void dr_click(int accent, uint32_t delay);   /* the metronome: a high tick on the bar's first beat */
void dr_choke(uint32_t inst);            /* a hand on the instrument: what it has sounding stops (in a few ms) */
void dr_panic(void);                     /* any context: silence now */
void dr_render(float *out, uint32_t n);  /* mono, about -1 .. 1 */
uint32_t dr_active(void);                /* voices sounding (the host's checks) */
float dr_bent(uint32_t inst);            /* the glide's pitch now, semitones (the host's checks) */
