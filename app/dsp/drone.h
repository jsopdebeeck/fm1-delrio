/* SPDX-License-Identifier: MPL-2.0 */
/* The drones, each played alone: the tanpura's four strings plucked in turn, buzzing on their bridge (jawari); and the
 * shruti box's reeds, the notes chosen held, its bellows breathing. Synthesised as they play: no samples. The main
 * loop sets them (dn_tanpura, dn_shruti); the audio interrupt renders them into the mix (dn_render).
 * Compiled apart at -O2 (fm1.toml o2): only fastmath.h, no libm, no doubles. */
#pragma once
#include <stdint.h>

typedef struct {
    int32_t sa;                          /* the tonic, MIDI */
    uint8_t first;                       /* the first string: 0 Pa (a fourth under Sa), 1 Ma (a fifth under), 2 Ni
                                          * (a semitone under) */
    float cycle_s;                       /* the four plucks and the pause before they start again, seconds */
    float level;                         /* 0 .. 1 */
} dn_tanpura_t;

void dn_init(float sample_rate);
/* main loop: on or off (off: the strings ring out), and how */
void dn_tanpura(int on, const dn_tanpura_t *p);
/* main loop: on or off, the notes held (bit i: MIDI low + i, 27 at most), the level 0..1, the bellows' breathing
 * 0..1 (0 steady) and their strokes' length, seconds (a push, then a pull, each one stroke) */
void dn_shruti(int on, uint32_t notes, int32_t low, float level, float bellows, float stroke_s);
void dn_panic(void);                     /* any context: silence now */
void dn_render(float *out, uint32_t n);  /* audio interrupt: added into out */
int dn_string(void);                     /* the tanpura's string plucked last, 0..3 (-1: none yet) */
