/* SPDX-License-Identifier: MPL-2.0 */
/* NEW: a rhythm box of ceremony percussion on the FM-1: djembe, cajon, udu, clapsticks, maracas and triangle; the
 * berimbau and its toques; two drones, the tanpura and the shruti box. The white keys are the selected instrument's
 * steps, the black keys play the instruments. This header is the app's
 * half of the contract with the platform (fm1-base: device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "NEW"
#define APP_SPLASH_RGB RGB(240, 140, 40)
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */
#define APP_TOUCH                        /* velocity from the keys (lib/touch.h) */

/* the store: the settings, the panel calibration (the platform's) and the rhythms (the FREE ones and the two chosen) */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_RHYTHMS, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u, 0xDA000u}

/* the contract: the platform calls these */
void app_init(void);                     /* boot: the engine, then the saved settings (or defaults) */
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);   /* audio interrupt: n stereo frames, 24-bit */
void app_panic(void);                    /* silence now (before an update rewrites the flash) */
void app_ui_init(void);                  /* the first screen, after the splash */
void app_frame(void);                    /* the UI: input and a redraw, about 60 times a second */
void app_input_only(void);               /* between frames: input only */
void app_say(const char *a, const char *b);   /* a short message on the screen and to a screen reader */

/* the host simulator's checks (host/plat_host.c "expect"): inst, rhythm, beats, div, bpm, swing, human, playing,
 * rec, count (count-in steps left), step (the step sounding), page, keypage, muted, dirty, voices, click, countin,
 * sN (the selected instrument's stroke at step N: 0 none, 1..3), vN (its velocity), flamN (a flam there), strokes
 * (its steps with a stroke), msg (a message on the screen), glowFX / glowLFO / glowOCT / glowARP (that button glows), wires
 * (its snare wires, %), tune (its tuning, tenths of a semitone), bend (the udu's bass glide now, tenths),
 * hitsI (instrument I's strokes played since boot), mode (FX: the kit 0, the berimbau 1, the tanpura 2, the shruti 3),
 * drone, sa, first, string, notes (the drones') */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
