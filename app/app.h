/* SPDX-License-Identifier: MPL-2.0 */
/* FM-1_DelRio, by Jess Del Rio: a rhythm box of ceremony percussion on the FM-1: djembe, cajon, udu, clapsticks, maracas and triangle; the
 * berimbau and its toques; two drones, the tanpura and the shruti box. The white keys are the selected instrument's
 * steps, the black keys play the instruments. This header is the app's
 * half of the contract with the platform (fm1-base: device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "DEL RIO"
#define APP_SPLASH_RGB RGB(240, 140, 40)
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */
#define APP_TOUCH                        /* velocity from the keys (lib/touch.h) */

/* the store: the settings, the panel calibration (the platform's) and the rhythms (the FREE ones) */
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
 * maracas (their type, 0 the shaker ..), marasound (the stroke of theirs sounding: 1 + its type, 0 none), maratake (the
 * take their next stroke plays),
 * hitsI (instrument I's strokes played since boot), mode (FX: the kit 0, the berimbau 1, the tanpura 2, the shruti 3),
 * drone, sa, first, string, notes, tpp, jawari, cycle, kind, root, shtempo (the drones'), list / listi / listn (the
 * presets' list, the one it shows, how many it has), savelist (it is SAVE's, the FREE places), intro (the opening playing), batt (the battery read: 0..3 bars, 4 USB, -1 none yet), battshown (it is on the screen),
 * meter (the time signature shown: 404 is 4/4, 1208 is 12/8), bpmshown (the tempo on the screen: bpm, in counts on the
 * berimbau's page, halved there), rise (the tempo rising), lad / ladrows (the ladainha: 1
 * its base, 2 the chula; the rows that have come in), vreq (VIRADA waits for its bar), vary (the rows varying this bar),
 * vph (the gunga's sourced phrase), gourd (the berimbau shown's gourd, % closed), next (a rhythm chosen waits for its
 * bar), stopat (PLAY waits for the bar's first step), bpmnow (the tempo playing, whole BPM), dronebg (the drone sounding,
 * shown or not) */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
