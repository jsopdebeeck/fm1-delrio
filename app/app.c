/* SPDX-License-Identifier: MPL-2.0 */
/* FM-1_DelRio: a rhythm box of ceremony percussion. Six instruments (djembe, cajon, udu, clapsticks, maracas, triangle)
 * each with its strokes; the berimbau's page (FX): the capoeira's bateria, three berimbaus (gunga, medio,
 * viola), caxixi, pandeiro, atabaque, agogo, reco-reco, with the toques (played as the kit is); and two drones (the
 * tanpura, the shruti box: their own screen, drone.c), alone or under the rhythm. A rhythm is a bar of 2 to 8 beats,
 * 2 to 4 steps a beat, and each row's strokes on its steps (a row an instrument of the mode's: ROW_INST). The white
 * keys are the selected instrument's steps (a press: its next stroke, then none); the black keys play the instruments,
 * grouped as they are: djembe 3, cajon 2, udu 3, clapsticks and maracas, triangle (the berimbau's: PAD). Each setting
 * has one control: SELECT the instrument, ALGORITHM the beats (2 to 8), PRESET the rhythm among those with that many
 * beats (each as it is written, at its tempo as last set; 7 and 2 FREE places, any beats, the empty ones hidden),
 * PLAY plays it in a loop (PLAY again stops on the next bar's first step; a third time, now), a rhythm chosen while
 * playing comes in on the next bar, REC records what the black keys play onto the nearest step (from a stop, after a
 * bar counted in), SEL mutes the instrument, EDIT twice clears it, SEQ taps the tempo (held: it rises), ARP held makes
 * the djembe's strokes flams (a step's, a black key's), on the berimbau's page the atabaque's keys the agogo and VIRADA
 * the ladainha, OCT- / OCT+ the tempo (5 BPM a press, 10 held; it glides there over a bar), LFO the presets listed one
 * by one (held: the one shown chosen), SAVE keeps the settings, a changed rhythm in a FREE place, MASTER the volume; a bar longer than the 16 white keys has them follow the step sounding. On the
 * home page KNOB 1-4 set the tempo, the steps a beat, the swing (with 2 or 4 steps a beat) and the human feel; ENV
 * opens the instrument's SOUND, with only what a player of it can change (its level; the djembe's, the udu's, the
 * atabaque's and the berimbaus' tuning; the djembe's, the cajon's, the maracas' and the triangle's decay; the cajon's
 * snare wires; the maracas' type; how far pressing the
 * udu's bass key lowers it, as a palm closing the pot's opening; how far a berimbau's gourd closes on the belly, and
 * how often it varies its toque), GLO the SETUP.
 * The sequencer runs in the audio interrupt, to the sample. Part of the unity build (device/unity.c,
 * host/plat_host.c), after the platform: gfx.c and announce.c are in scope. */
#include "fastmath.h"
#include "drums.h"
#include "drone.h"

/* ------------------------------------------------------------ the instruments --- */
#define NINST DR_NINST
typedef struct {
    const char *name;
    uint16_t rgb;
    uint8_t nart;
    char letter[DR_NART];                /* on the steps, and in the rhythms below */
    const char *word[DR_NART];           /* the stroke's name, on a step wide enough for it (0: its letter) */
    int8_t pitch;                        /* semitones over its strokes as recorded (the medio, the viola: the gunga's) */
    dr_inst_t def;                       /* the sound at first: level, tune, decay, wires */
} inst_t;
static const inst_t INST[NINST] = {
    {"DJEMBE", RGB(240, 140, 40), 3, {'B', 'T', 'S'}, {0}, 0, {0.85f, 0.0f, 1.0f, 1.0f}},
    {"CAJON", RGB(236, 206, 72), 2, {'B', 'S', 0}, {0}, 0, {0.9f, 0.0f, 1.0f, 1.0f}},
    {"UDU", RGB(222, 82, 60), 3, {'B', 'T', 'S'}, {0}, 0, {0.85f, 0.0f, 1.0f, 1.0f}},
    {"CLAPSTICKS", RGB(124, 204, 84), 1, {'X', 0, 0}, {0}, 0, {0.65f, 0.0f, 1.0f, 1.0f}},
    {"MARACAS", RGB(232, 96, 184), 1, {'X', 0, 0}, {0}, 0, {0.7f, 0.0f, 1.0f, 1.0f}},
    {"TRIANGLE", RGB(92, 192, 242), 2, {'O', 'M', 0}, {0}, 0, {0.5f, 0.0f, 1.0f, 1.0f}},
    /* the berimbau's (FX): the wire struck free (DOM, O), the stone pressed on it (DIM, I: a tone higher), the stone
     * resting on it (TSCH, X: a buzz); the caxixi in the same hand. The gunga, the lowest of the roda's three */
    {"GUNGA", RGB(64, 200, 120), 3, {'O', 'I', 'X'}, {"DOM", "DIM", "TCH"}, 0, {0.85f, 0.0f, 1.0f, 1.0f}},
    {"CAXIXI", RGB(244, 204, 64), 1, {'X', 0, 0}, {0}, 0, {0.7f, 0.0f, 1.0f, 1.0f}},
    /* the agogo's two bells, high and low: the capoeira's bateria only */
    {"AGOGO", RGB(196, 200, 214), 2, {'H', 'L', 0}, {"HIGH", "LOW", 0}, 0, {0.6f, 0.0f, 1.0f, 1.0f}},
    /* the rest of the capoeira's bateria: the medio and the viola (the gunga's strokes, higher), the pandeiro (its
     * thumb's TUM, its slap PA, its jingles CHIA), the atabaque (its bass TUM, its open PA), the reco-reco */
    /* tuned as a bateria commonly is, each one's DIM the next one's DOM: a tone up each (opencapoeira.com, "Tuning
     * berimbaus in a bateria", 2025) */
    {"MEDIO", RGB(40, 176, 176), 3, {'O', 'I', 'X'}, {"DOM", "DIM", "TCH"}, 2, {0.75f, 0.0f, 1.0f, 1.0f}},
    {"VIOLA", RGB(150, 222, 90), 3, {'O', 'I', 'X'}, {"DOM", "DIM", "TCH"}, 4, {0.7f, 0.0f, 1.0f, 1.0f}},
    {"PANDEIRO", RGB(240, 150, 60), 3, {'T', 'P', 'C'}, {"TUM", "PA", "CHIA"}, 0, {0.7f, 0.0f, 1.0f, 1.0f}},
    {"ATABAQUE", RGB(206, 92, 64), 2, {'T', 'P', 0}, {"TUM", "PA", 0}, 0, {0.8f, 0.0f, 1.0f, 1.0f}},
    {"RECO-RECO", RGB(186, 140, 240), 1, {'R', 0, 0}, {0}, 0, {0.55f, 0.0f, 1.0f, 1.0f}},
};
enum { I_DJEMBE, I_CAJON, I_UDU, I_CLAPSTICKS, I_MARACAS, I_TRIANGLE, I_BERIMBAU, I_CAXIXI, I_AGOGO, I_MEDIO, I_VIOLA,
       I_PANDEIRO, I_ATABAQUE, I_RECO, I_NONE = 0xFF };
#define NINST_V09 8u                     /* 0.9's instruments: the kit's six, the berimbau and the caxixi */
/* a rhythm's rows of steps: the kit's six, nothing else in the kit (1.0's agogo row, no more: the store's records have
 * its place); the berimbau's, the gunga and the caxixi where 0.9 had them (its toques kept as they were), the rest of
 * the bateria before them */
#define NROW 8u
enum { RB_MEDIO, RB_VIOLA, RB_PANDEIRO, RB_ATABAQUE, RB_AGOGO, RB_RECO, RB_GUNGA, RB_CAXIXI };
static const uint8_t ROW_INST[2][NROW] = {
    {I_DJEMBE, I_CAJON, I_UDU, I_CLAPSTICKS, I_MARACAS, I_TRIANGLE, I_NONE, I_NONE},
    {I_MEDIO, I_VIOLA, I_PANDEIRO, I_ATABAQUE, I_AGOGO, I_RECO, I_BERIMBAU, I_CAXIXI},
};
/* SELECT goes through them in this order: the kit's; the berimbau's, the three berimbaus first */
static const uint8_t ROW_ORDER[2][NROW] = {{0, 1, 2, 3, 4, 5, 0xFF, 0xFF},
                                           {RB_GUNGA, RB_MEDIO, RB_VIOLA, RB_CAXIXI, RB_PANDEIRO, RB_ATABAQUE,
                                            RB_AGOGO, RB_RECO}};
static const uint8_t ROW_N[2] = {6, 8};
/* the black keys, left to right: row and stroke. The kit's; the berimbau's: the berimbau shown (the gunga, the
 * medio or the viola) DOM DIM TSCH, the caxixi and the reco-reco, the pandeiro TUM PA CHIA, the atabaque TUM PA,
 * and VIRADA: the berimbau shown varies the next bar. With ARP held, the atabaque's two are the agogo's high and low
 * bells */
#define PAD_SHOWN 0xF0u                  /* the berimbau shown */
#define PAD_VIRADA 0xF1u
static const uint8_t PAD[2][11][2] = {
    {{0, 1}, {0, 2}, {0, 3}, {1, 1}, {1, 2}, {2, 1}, {2, 2}, {2, 3}, {3, 1}, {4, 1}, {5, 1}},
    {{PAD_SHOWN, 1}, {PAD_SHOWN, 2}, {PAD_SHOWN, 3}, {RB_CAXIXI, 1}, {RB_RECO, 1}, {RB_PANDEIRO, 1},
     {RB_PANDEIRO, 2}, {RB_PANDEIRO, 3}, {RB_ATABAQUE, 1}, {RB_ATABAQUE, 2}, {PAD_VIRADA, 1}},
};
/* the gunga's, the medio's, the viola's place in set.virada (-1: not a berimbau) */
static int ber_idx(uint32_t i) { return i == I_BERIMBAU ? 0 : i == I_MEDIO ? 1 : i == I_VIOLA ? 2 : -1; }
#define AGOGO_PAD 8u                     /* the berimbau's, with ARP: this key and the next, the agogo's high and low */
static const uint8_t WHITE[16] = PLAT_WHITE_KEYS, BLACK[11] = PLAT_BLACK_KEYS;
#define TRIANGLE_PAD 10u                 /* held, the triangle rings; let go, the hand closes on it */
#define TAP_MS 180u                      /* a triangle key let go sooner: a muted stroke (REC records it so) */
#define UDU_BASS_PAD 5u                  /* the black key of the udu's bass: its pressure lowers it */
#define FLAM_S 0.028f                    /* a flam: a soft grace stroke, the stroke this long after */
#define FLAM_GRACE 0.45f

/* ---------------------------------------------------------------- the rhythms --- */
#define NSTEPS 32u                       /* 8 beats of 4 */
#define NRHY_KIT 39u                     /* the kit's rhythms, then the berimbau's toques */
#define NRHY 52u                         /* (RODA ANGOLA after 0.9's 51: their places kept) */
#define ART(v) ((uint32_t)(v) >> 6)      /* a step: its stroke (0 none, 1..3) in the top two bits, */
#define FLAM(v) ((uint32_t)(v) >> 5 & 1u)   /* a flam (the djembe's) in the next, */
#define VEL(v) (((uint32_t)(v) & 31u) * 4u + 3u)   /* its velocity (3..127) below */
#define STEP(art, vel, flam) (uint8_t)((art) << 6 | (flam) << 5 | ((vel) >> 2 & 31u))
typedef struct {
    uint8_t beats;                       /* 2 .. 8 */
    uint8_t div;                         /* steps a beat: 2 .. 4 */
    uint8_t bpm;                         /* 40 .. 240 */
    uint8_t swing;                       /* 0 .. 100 %: 100 the off-steps a triplet late */
    uint8_t human;                       /* 0 .. 100 %: the strokes' timing and strength loosened */
    uint8_t mute;                        /* bit i: row i silent */
    uint8_t rsv[2];
    uint8_t step[NROW][NSTEPS];          /* each row's (its instrument the mode's: ROW_INST) */
} rhythm_t;
static rhythm_t rhy[NRHY];

#include "rhythms.h"                     /* RDEF: the rhythms as written, with their sources */
typedef char rdef_one_each[sizeof RDEF / sizeof RDEF[0] == NRHY ? 1 : -1];   /* a rhythm for each place */

/* kept in the store: the FREE rhythms; the others come as written */
#define NKEEP 12u
#define RHY_MAGIC 0x38594852u            /* "RHY8": the rhythms' places changed with 0.8 */
typedef struct {
    uint32_t magic;
    uint8_t which[NKEEP];                /* each one's place in rhy (0xFF: none) */
    rhythm_t r[NKEEP];
} keep_t;
static keep_t keep;
typedef char keep_fits[sizeof(keep_t) <= 3840u ? 1 : -1];   /* one store object */

/* -------------------------------------------------------------- the settings --- */
enum { PG_HOME, PG_SOUND, PG_SETUP, NPAGES };
static const char *const PAGE_NAME[NPAGES] = {"HOME", "SOUND", "SETUP"};
static const uint8_t PAGE_BTN[NPAGES] = {B_HOME, B_ENV, B_GLO};
/* FX: the kit; the berimbau, played alone as the kit is (its strokes, its toques); or one of the drones played alone,
 * each with the whole FM-1 */
enum { MODE_KIT, MODE_BERIMBAU, MODE_TANPURA, MODE_SHRUTI, NMODES };
static const char *const MODE_NAME[NMODES] = {"KIT", "BERIMBAU", "TANPURA", "SHRUTI"};
static const uint16_t MODE_RGB[NMODES] = {RGB(240, 140, 40), RGB(64, 200, 120), RGB(168, 124, 240),
                                          RGB(72, 204, 176)};
static uint32_t rhy_mode(uint32_t k) { return k >= NRHY_KIT ? MODE_BERIMBAU : MODE_KIT; }   /* a rhythm's */
static int inst_in(uint32_t i, uint32_t mode)   /* a row of the kit's, or of the berimbau's */
{
    return i < ROW_N[mode == MODE_BERIMBAU];
}
static uint32_t row_inst(uint32_t k, uint32_t row) { return ROW_INST[rhy_mode(k) == MODE_BERIMBAU][row]; }   /* rhythm k's */
static const char *const NOTE_ABC[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
enum { CLICK_OFF, CLICK_REC, CLICK_ON };
#define SET_MAGIC 0x39534452u            /* "RDS9": the settings record; another magic loads defaults (0.8's
                                          * still loads: the store gives its first sizeof bytes) */
typedef struct {
    uint32_t magic;
    uint8_t rhythm, inst, page;          /* the rhythm and the instrument shown (the kit's, or the berimbau's) */
    uint8_t click;                       /* the metronome: off, while recording (and counting in), always */
    uint8_t countin;                     /* REC from a stop counts a bar in first */
    uint8_t lights;                      /* the empty steps' lights: 1 low, 2 high */
    uint8_t fixed;                       /* the keys' velocity ignored: every stroke at 100 */
    uint8_t bend;                        /* the udu's bass key pressed all the way: this many semitones lower, 0 .. 6 */
    dr_inst_t snd[NINST];
    uint8_t mode;                        /* the kit, or a drone (FX) */
    uint8_t tp_sa;                       /* the tanpura: its Sa's note, 0 C .. 11 B */
    uint8_t tp_first;                    /* its first string: 0 Pa, 1 Ma, 2 Ni */
    uint8_t tp_high;                     /* its register: 0 low (Sa C3 .. B3), 1 high (C4 .. B4) */
    uint8_t tp_cycle;                    /* its cycle, tenths of a second, 20 .. 80 */
    uint8_t tp_level, sh_level, sh_bellows;   /* the tanpura's and the shruti's level, the shruti's breathing (its
                                              * bellows: 0 steady): % */
    uint32_t sh_notes;                   /* the shruti's notes held: bit k, the FM-1's key k */
    uint8_t other_rhythm, other_inst;    /* the other's: the berimbau's while the kit is shown, the kit's while not */
    uint8_t tp_preset;                   /* the tanpura's character chosen last (PRESETS) */
    uint8_t tp_jawari;                   /* its strings' buzz on the bridge: 0 closed .. 100 open, % */
    uint8_t sh_kind, sh_root;            /* the shruti's: its reeds one by one, or a chord (PRESETS) on a key */
    uint8_t sh_tempo;                    /* its bellows' strokes a minute (a push, a pull), 20 .. 120 */
    /* after 0.9's record (which loads with what follows as at first) */
    uint8_t bpm[NRHY];                   /* each rhythm's tempo as last set (0: as written): it comes back with it */
    dr_inst_t snd_x[NINST - NINST_V09];  /* the instruments after 0.9's: their sounds */
    uint8_t cabaca;                      /* the berimbaus' gourds: how far a key pressed all the way closes them, % */
    uint8_t virada[3];                   /* the gunga's, the medio's, the viola's variations: 0 none, 1 every 8 bars,
                                          * 2 every 4, 3 every 2, 4 every bar */
    uint8_t maracas;                     /* the maracas' type: MARACAS_NAME's */
} settings_t;
#define SET_V09 __builtin_offsetof(settings_t, bpm)   /* 0.9's record, this long */
static settings_t set;
#define NMARACAS 4u                      /* their types: kit.py's strokes of the maracas, in its order */

/* the stroke an instrument's step or key plays: the maracas have one, in the type chosen */
static uint32_t stroke_of(uint32_t in, uint32_t art)
{
    return in == I_MARACAS ? set.maracas + 1u : art;
}
static uint8_t set_dirty;                /* changed since the last save: SAVE is lit */

static void rhythm_build(uint32_t k, rhythm_t *r)
{
    const rdef_t *d = &RDEF[k];
    uint32_t i, s, a;
    for (i = 0; i < sizeof *r; i++)
        ((uint8_t *)r)[i] = 0;
    r->beats = d->beats;
    r->div = d->div;
    r->bpm = set.bpm[k] ? set.bpm[k] : d->bpm;
    r->swing = d->swing;
    for (i = 0; i < NROW; i++) {
        const inst_t *in = row_inst(k, i) < NINST ? &INST[row_inst(k, i)] : 0;
        for (s = 0; in && s < NSTEPS && d->pat[i] && d->pat[i][s]; s++)
            for (a = 0; a < in->nart; a++) {
                char c = d->pat[i][s];
                if (c == in->letter[a])
                    r->step[i][s] = STEP(a + 1u, 112u, 0u);
                else if (c == in->letter[a] + 32)   /* small: soft */
                    r->step[i][s] = STEP(a + 1u, 70u, 0u);
                else if (c == 'F' && a + 1u == in->nart)   /* the slap, a flam */
                    r->step[i][s] = STEP(a + 1u, 112u, 1u);
            }
    }
}

static void rhythm_default(uint32_t k)
{
    rhythm_t one;                        /* built aside, then put in place at once: it may be the one playing */
    uint32_t i;
    rhythm_build(k, &one);
    i = plat_lock();
    rhy[k] = one;
    plat_unlock(i);
}

static rhythm_t *cur(void) { return &rhy[set.rhythm]; }
static dr_inst_t *snd_of(uint32_t i) { return i < NINST_V09 ? &set.snd[i] : &set.snd_x[i - NINST_V09]; }
static uint32_t sinst(void) { return row_inst(set.rhythm, set.inst); }   /* the instrument shown */
static const inst_t *shown(void) { return &INST[sinst()]; }
static int berimbau(void) { return set.mode == MODE_BERIMBAU; }
static int drone(void) { return set.mode >= MODE_TANPURA; }
static uint32_t nsteps(const rhythm_t *r) { return (uint32_t)r->beats * r->div; }
/* the berimbau's page counts as its sources do: a beat is the toque's count, an eighth of its 2/4 bar. Its tempo is kept
 * and played in counts a minute, and shown in quarter notes: ANGOLA's 90, 45 BPM */
static uint32_t bpm_per(const rhythm_t *r) { return rhy_mode((uint32_t)(r - rhy)) == MODE_BERIMBAU ? 2u : 1u; }
static uint32_t bpm_shown(const rhythm_t *r) { return ((uint32_t)r->bpm + bpm_per(r) / 2u) / bpm_per(r); }
/* the bar as a time signature: its beats over 4; beats of three steps, their eighths over 8. The berimbau's: its counts
 * in quarter notes, four or eight of them 2/4 (one bar or two), an odd number over 8 */
static uint32_t meter_top(const rhythm_t *r)
{
    if (bpm_per(r) == 2u)
        return r->beats % 2u ? r->beats : r->beats % 4u ? r->beats / 2u : 2u;
    return r->div == 3u ? r->beats * 3u : r->beats;
}

static uint32_t meter_low(const rhythm_t *r)
{
    if (bpm_per(r) == 2u)
        return r->beats % 2u ? 8u : 4u;
    return r->div == 3u ? 8u : 4u;
}
/* the keys' page: as many whole beats as the 16 white keys hold */
static uint32_t page_steps(const rhythm_t *r) { return 16u / r->div * r->div; }
static uint32_t npages(const rhythm_t *r) { return (nsteps(r) + page_steps(r) - 1u) / page_steps(r); }

/* ------------------------------------------------------------- the sequencer --- */
/* in the audio interrupt: each block, the steps that fall in it start their strokes at their sample (dr_hit's delay) */
#define SR 44100.0f
static struct {
    volatile uint8_t playing, rec;
    volatile uint32_t count;             /* count-in steps left (clicks only) */
    volatile int32_t step;               /* the step sounding (-1 before the first) */
    volatile float left, len;            /* samples to the next step, and the step's length */
    volatile uint8_t skip[NROW];         /* 1 + a step: that row's next stroke there is already played */
    volatile uint32_t hits[NINST];       /* strokes played, each instrument (the pads' lights, the checks) */
    volatile uint32_t beats;             /* beats played (SEQ's light) */
    volatile uint8_t next;               /* 1 + a rhythm chosen while playing: it comes in on the next bar's first step */
    volatile uint8_t vreq;               /* 1 + a berimbau's row: VIRADA, it varies the next bar */
    uint8_t vary;                        /* the rows varying this bar (bit a row), */
    uint8_t vkind;                       /* how (made from the toque), */
    uint8_t vph[3], vbar[3];             /* or the gunga's, the medio's, the viola's sourced phrase (1 + its set << 3
                                          * | its place; 0 none) and its bar */
    uint32_t bar;                        /* bars played since PLAY */
    volatile uint8_t stop_at;            /* PLAY while playing: the next bar's first step is the last */
    volatile uint8_t jump;               /* the tempo at once (tapped, a rhythm come in), not glided */
    volatile uint32_t switched;          /* rhythms come in (the main loop shows the new one) */
    volatile uint8_t lad;                /* the ladainha (ARP + VIRADA): 0 none, 1 its base, 2 the chula, all of them */
    volatile uint8_t lad_rows;           /* the rows that have come in (0: none yet, all sound) */
    uint8_t lad_step, lad_bar;           /* the next to come in, and the bars since it began */
    float bpm_now, bpm_inc;              /* the tempo playing, gliding to the rhythm's over a bar */
    uint8_t bpm_to;
    uint32_t rnd;
} seq;
static struct {                          /* the black keys' strokes (0: a hand on it), from the main loop to the audio */
    uint8_t inst[16], art[16], vel[16];
    uint16_t delay[16];
    volatile uint32_t w, r;
} padq;

static float rnd01(void)
{
    seq.rnd = seq.rnd * 1664525u + 1013904223u;
    return (float)(seq.rnd >> 8) * (1.0f / 16777216.0f);
}

/* the tempo changed while playing glides there over a bar (OCT-, OCT+, KNOB 1: no jolt under the singing); tapped, or
 * a rhythm come in, it is there at once. A step at a time */
static void tempo_step(const rhythm_t *r, uint32_t ns)
{
    if (seq.jump || seq.bpm_now < 1.0f) {
        seq.jump = 0;
        seq.bpm_now = (float)r->bpm;
        seq.bpm_to = r->bpm;
        seq.bpm_inc = 0.0f;
        return;
    }
    if (r->bpm != seq.bpm_to) {
        seq.bpm_to = r->bpm;
        seq.bpm_inc = ((float)r->bpm - seq.bpm_now) / (float)ns;
    }
    if (seq.bpm_inc != 0.0f) {
        seq.bpm_now += seq.bpm_inc;
        if ((seq.bpm_inc > 0.0f) == (seq.bpm_now >= (float)seq.bpm_to)) {
            seq.bpm_now = (float)seq.bpm_to;
            seq.bpm_inc = 0.0f;
        }
    }
}

static float step_len(const rhythm_t *r, uint32_t s)   /* samples; with swing, the off-steps late */
{
    float l = SR * 60.0f / (seq.bpm_now * (float)r->div);
    if (r->div % 2u == 0u && r->swing) {
        float w = (float)r->swing * (1.0f / 300.0f);   /* 100 %: a third of a step */
        l *= s & 1u ? 1.0f - w : 1.0f + w;
    }
    return l;
}

static int click_now(void)
{
    return set.click == CLICK_ON || (set.click == CLICK_REC && (seq.rec || seq.count));
}

/* a berimbau's variation of its toque (VIRADAS, VIRADA). For the two toques the sources write variations of, one of
 * theirs, two bars long (counted as the toques, 0 the chiados; a DOM closed at once into a chiado, "dom-ch", a DOM and
 * a soft TSCH): ANGOLA's, the viola's "common variations" in Wikipedia's "Capoeira toques"; SAO BENTO GRANDE de
 * Angola's, Elias's "So Ritmo" (2017), its "Viradas". Else one bar made from the toque, not transcribed: DOBRADO its
 * DOM and DIM in the bar's second half struck twice, REPIQUE the last beat's steps all DIM, CHIADO the second half's
 * empty steps a soft TSCH */
static const struct {
    const char *base;                    /* the toque, as the row has it (4 beats of 2) */
    const char *phrase[6];               /* its variations, two bars each */
} VSRC[2] = {
    {"xxO.I...", {"xxO.xxI.xxO.I...", "xxO.IOIO.xO.I...", 0}},
    {"xxI.O.O.", {"xxI.O.OxxOI.O.O.", "xxI.O..O.OI.O.O.", "xxI.O..O.OI.OIO.", "xxI.OxOxOxIxOxOx", "xxI.O.O.IOI.O.O.", 0}},
};
enum { VK_DOBRADO, VK_REPIQUE, VK_CHIADO, NVK };

static uint32_t vstep(char c)            /* a phrase's step: DOM O, DIM I, TSCH X; small soft; "." none */
{
    uint32_t a = c == 'O' || c == 'o' ? 1u : c == 'I' || c == 'i' ? 2u : c == 'X' || c == 'x' ? 3u : 0u;
    return a ? STEP(a, c >= 'a' ? 70u : 112u, 0u) : 0u;
}

static int vsrc_of(const rhythm_t *r, uint32_t row)   /* the sourced variations of the row's toque (-1: none) */
{
    uint32_t k, s;
    if (nsteps(r) != 8u)
        return -1;
    for (k = 0; k < 2u; k++) {
        for (s = 0; s < 8u && ART(r->step[row][s]) == ART(vstep(VSRC[k].base[s])); s++)
            ;
        if (s == 8u)
            return (int)k;
    }
    return -1;
}
static uint32_t vary(const rhythm_t *r, uint32_t row, uint32_t s)
{
    uint32_t ns = nsteps(r), v = r->step[row][s], p, g = row == RB_GUNGA ? 0u : row == RB_MEDIO ? 1u : 2u;
    if (seq.vph[g])                      /* a sourced phrase, its bar (8 steps: vsrc_of) */
        return s >= 8u ? v : vstep(VSRC[(seq.vph[g] - 1u) >> 3].phrase[(seq.vph[g] - 1u) & 7u][seq.vbar[g] * 8u + s]);
    switch (seq.vkind) {
    case VK_DOBRADO:
        p = s ? r->step[row][s - 1u] : 0u;
        return !ART(v) && s >= ns / 2u && (ART(p) == 1u || ART(p) == 2u) ? STEP(ART(p), 70u, 0u) : v;
    case VK_REPIQUE:
        return s >= ns - r->div ? STEP(2u, s == ns - r->div ? 112u : 80u, 0u) : v;
    default:
        return !ART(v) && s >= ns / 2u ? STEP(3u, 60u, 0u) : v;
    }
}

/* the ladainha, on RODA ANGOLA: the gunga with its caxixi begins, then the medio, the viola, the pandeiro, two bars
 * each; the chula, on its bar: the atabaque, the agogo, the reco-reco (L. V. Xavier, "O canto que embala a roda de
 * capoeira angola", Revista Africa e Africanidades 10, 2010, p. 3; the bars each are ours) */
#define R_RODA (NRHY - 1u)
#define LAD_BASE (1u << RB_GUNGA | 1u << RB_CAXIXI | 1u << RB_MEDIO | 1u << RB_VIOLA | 1u << RB_PANDEIRO)
static int lad_out(uint32_t row) { return seq.lad && seq.lad_rows && !(seq.lad_rows >> row & 1u); }   /* not in yet */

static void lad_bar(void)
{
    static const uint8_t ENTER[4] = {1u << RB_GUNGA | 1u << RB_CAXIXI, 1u << RB_MEDIO, 1u << RB_VIOLA, 1u << RB_PANDEIRO};
    if (seq.lad == 2u)
        seq.lad_rows = 0xFFu;
    else if (seq.lad == 1u && seq.lad_step >= 4u)
        seq.lad_rows = LAD_BASE;
    else if (seq.lad == 1u && !(seq.lad_bar++ % 2u))
        seq.lad_rows |= ENTER[seq.lad_step++];
}

/* the bar's first step: which berimbaus vary it (each every so many bars, VIRADAS; the one VIRADA asked for)*/
static void bar_start(void)
{
    static const uint8_t EVERY[5] = {0, 8, 4, 2, 1};
    static const uint8_t ROWS[3] = {RB_GUNGA, RB_MEDIO, RB_VIOLA};
    const rhythm_t *r = cur();
    uint32_t i, q = seq.vreq, start = 0;
    lad_bar();
    seq.bar++;
    seq.vary = 0;
    for (i = 0; i < 3u; i++)             /* a sourced phrase goes on into its second bar */
        if (seq.vph[i] && ++seq.vbar[i] >= 2u)
            seq.vph[i] = 0;
    if (rhy_mode(set.rhythm) != MODE_BERIMBAU) {
        seq.vreq = 0;
        for (i = 0; i < 3u; i++)
            seq.vph[i] = 0;
        return;
    }
    for (i = 0; i < 3u; i++) {
        uint32_t e = EVERY[set.virada[i]];
        if (e && seq.bar % e == 0u)
            start |= 1u << i;
    }
    if (q) {
        seq.vreq = 0;
        start |= 1u << (q - 1u == RB_GUNGA ? 0u : q - 1u == RB_MEDIO ? 1u : 2u);
    }
    for (i = 0; i < 3u; i++) {
        int k;
        if (seq.vph[i]) {
            seq.vary |= (uint8_t)(1u << ROWS[i]);
            continue;
        }
        if (!(start >> i & 1u))
            continue;
        seq.vary |= (uint8_t)(1u << ROWS[i]);
        if ((k = vsrc_of(r, ROWS[i])) >= 0) {   /* one of the sources' phrases */
            uint32_t n = 0;
            while (VSRC[k].phrase[n])
                n++;
            seq.vph[i] = (uint8_t)(((uint32_t)k << 3 | (uint32_t)(rnd01() * (float)n) % n) + 1u);
            seq.vbar[i] = 0;
        }
    }
    if (seq.vary)
        seq.vkind = (uint8_t)((uint32_t)(rnd01() * (float)NVK) % NVK);
}

static void seq_next(uint32_t at)        /* the next step starts at sample at of this block */
{
    const rhythm_t *r = cur();
    uint32_t ns = nsteps(r), s, i;
    if (seq.count) {                     /* counting in: a bar of clicks */
        s = ns - seq.count;
        if (s % r->div == 0u) {
            dr_click(s == 0u, at);
            seq.beats++;
        }
        seq.count--;
        tempo_step(r, ns);
        seq.len = step_len(r, s);
        return;
    }
    s = seq.step < 0 || (uint32_t)seq.step + 1u >= ns ? 0u : (uint32_t)seq.step + 1u;
    if (s == 0u && seq.next) {           /* the rhythm chosen comes in, on its first step, at its tempo */
        set.rhythm = (uint8_t)(seq.next - 1u);
        if (set.rhythm != R_RODA)
            seq.lad = 0;
        seq.next = 0;
        seq.jump = 1;
        for (i = 0; i < NROW; i++)
            seq.skip[i] = 0;
        seq.vph[0] = seq.vph[1] = seq.vph[2] = 0;   /* (a variation was the old toque's) */
        seq.switched++;
        r = cur();
        ns = nsteps(r);
    }
    seq.step = (int32_t)s;
    tempo_step(r, ns);
    seq.len = step_len(r, s);
    if (s == 0u)
        bar_start();
    if (s % r->div == 0u) {
        seq.beats++;
        if (click_now())
            dr_click(s == 0u, at);
        dr_down(I_MARACAS);              /* the beat's first stroke the same each beat: their takes in turn would pair
                                          * the beats, a pulse at half the tempo */
    }
    for (i = 0; i < NROW; i++) {
        uint32_t v = seq.vary >> i & 1u ? vary(r, i, s) : r->step[i][s], d = at, in = row_inst(set.rhythm, i);
        float vel;
        if (!ART(v) || in >= NINST)
            continue;
        if (seq.skip[i] == s + 1u) {     /* recorded ahead of the step: it was played as it was played */
            seq.skip[i] = 0;
            continue;
        }
        if ((r->mute >> i & 1u) || lad_out(i))
            continue;
        vel = (float)VEL(v) * (1.0f / 127.0f);
        if (r->human) {                  /* up to 12 ms late and 30 % softer */
            float h = (float)r->human * 0.01f;
            d += (uint32_t)(rnd01() * h * 0.012f * SR);
            vel *= 1.0f - rnd01() * h * 0.3f;
        }
        if (FLAM(v)) {                   /* the grace stroke on the step, the stroke just after */
            dr_hit(in, stroke_of(in, ART(v)), vel * FLAM_GRACE, d);
            d += (uint32_t)(FLAM_S * SR);
        }
        dr_hit(in, stroke_of(in, ART(v)), vel, d);
        seq.hits[in]++;
    }
    if (s == 0u && seq.stop_at) {        /* the bar's first step played: the last */
        seq.playing = 0;
        seq.rec = 0;
        seq.stop_at = 0;
    }
}

static void seq_start(int count_in)
{
    uint32_t i;
    for (i = 0; i < NROW; i++)
        seq.skip[i] = 0;
    seq.step = -1;
    seq.left = 0.0f;
    seq.next = 0;
    seq.stop_at = 0;
    seq.jump = 1;
    seq.lad = 0;
    seq.bar = 0;
    seq.vary = 0;
    seq.vph[0] = seq.vph[1] = seq.vph[2] = 0;
    tempo_step(cur(), nsteps(cur()));
    seq.len = step_len(cur(), 0);
    seq.count = count_in ? nsteps(cur()) : 0u;
    seq.playing = 1;
}

static void seq_stop(void)              /* now */
{
    seq.lad = 0;
    seq.playing = 0;
    seq.rec = 0;
    seq.count = 0;
    seq.next = 0;
    seq.stop_at = 0;
}

static uint32_t rhythm_shown(void)       /* the rhythm chosen: the one coming in at the next bar, or the one playing */
{
    uint32_t n = seq.next;
    return n ? n - 1u : set.rhythm;
}

/* ------------------------------------------------------------------- audio --- */
static float gain_now;                   /* glides to MASTER: no step at a block's edge */

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    static float buf[APP_BLOCK];
    float gain = (float)master_q12 * (1.0f / 4096.0f) * 7000000.0f;   /* MASTER, read once a block */
    uint32_t i, done = 0;
    while (padq.r != padq.w) {           /* the black keys played since the last block */
        uint32_t k = padq.r % 16u;
        __asm__ volatile("" ::: "memory");
        if (!padq.art[k])                /* stroke 0: a hand on it */
            dr_choke(padq.inst[k]);
        else {
            dr_hit(padq.inst[k], stroke_of(padq.inst[k], padq.art[k]), (float)padq.vel[k] * (1.0f / 127.0f), padq.delay[k]);
            seq.hits[padq.inst[k]]++;
        }
        padq.r++;
    }
    while (done < n) {
        uint32_t m = n - done < APP_BLOCK ? n - done : APP_BLOCK;
        if (seq.playing) {               /* the steps that start in these m samples */
            float t = seq.left;
            if ((int32_t)seq.step >= (int32_t)nsteps(cur()))
                seq.step = -1;           /* the bar got shorter */
            while (seq.playing && t < (float)m) {   /* (a stop on the bar's first step ends it) */
                seq_next((uint32_t)t);
                t += seq.len;
            }
            seq.left = t - (float)m;
        }
        dr_render(buf, m);
        dn_render(buf, m);
        for (i = 0; i < m; i++) {
            int32_t v;
            gain_now += (gain - gain_now) * 0.004f;
            v = (int32_t)fm_clampf(buf[i] * gain_now, -8300000.0f, 8300000.0f);
            lr[2u * (done + i)] = v;
            lr[2u * (done + i) + 1u] = v;
        }
        done += m;
    }
    if (fm_fabsf(gain - gain_now) < 1.0f)   /* there: no lingering in the smallest floats */
        gain_now = gain;
}


/* -------------------------------------------------------------------- text --- */
static struct {
    char msg[2][24];                     /* a short message over the header */
    uint32_t msg_until;
    uint16_t msg_rgb;
    uint8_t msg_title;                   /* the message is a rhythm's name: as large as a title */
    uint32_t keys, btn;
    uint8_t keypage;                     /* the white keys' page of steps */
    uint32_t clear_t;                    /* EDIT once: a second press within 2 s clears the instrument */
    uint32_t tap[4];                     /* SEQ's last taps */
    uint8_t ntap;
    uint32_t hits_seen[NINST], lit_until[NINST];   /* a pad lights when its instrument sounds */
    uint32_t beats_seen, beat_until;
    int32_t drawn_step;
    uint32_t switched;                   /* seq.switched seen */
    uint32_t bellows_t;                  /* the shruti's bellows drawn last */
    float bend;                          /* the udu's bass, from its key's pressure: semitones */
    float gourd;                         /* the berimbau shown's gourd, from its keys' pressure: 0 open .. 1 closed */
    uint8_t gourd_inst;
    uint32_t tri_down;                   /* the triangle's key: when it went down, */
    int32_t tri_step;                    /* the step it recorded (-1: none), */
    uint8_t tri_rhythm;                  /* in which rhythm */
    uint32_t glow;                       /* the buttons glowing at the last input (the host's checks) */
    uint8_t drone;                       /* the drone sounding (its PLAY): MODE_TANPURA, MODE_SHRUTI; 0 none. It goes on
                                          * under the kit or the berimbau */
    int8_t string_drawn;                 /* the tanpura's string the screen shows plucked */
    int8_t batt;                         /* the battery shown (plat_battery's: -1 not read yet), */
    uint8_t batt_off;                    /* the keys' readings let go for its reading, */
    uint32_t batt_t;                     /* since */
    uint32_t batt_until;                 /* the battery on the screen until then (HOME, FX) */
    uint8_t rise;                        /* the tempo rising (SEQ held) on this rhythm, */
    uint8_t rise_rhythm, rise_from;      /* from this tempo, */
    uint32_t rise_beats;                 /* the beats played at its last BPM */
    uint32_t down_at[4];                 /* OCT-, OCT+, LFO, SEQ: when each went down; */
    uint8_t held_long;                   /* each held long enough (its long press done) */
    uint8_t list_i;                      /* the presets' list (LFO): the one shown, */
    uint8_t list_on;                     /* the list on the screen */
    uint8_t list_save;                   /* it is SAVE's: the FREE places */
    uint8_t intro;                       /* the opening is playing (app_ui_init), since intro_t0 */
    uint32_t intro_t0;
    int32_t intro_drawn;                 /* its step on the screen */
    uint8_t redraw;
} ui;

static int drone_on(void) { return ui.drone && ui.drone == set.mode; }   /* the drone shown sounds */

void app_panic(void)
{
    seq_stop();
    dr_panic();
    dn_panic();
    ui.drone = 0;                        /* (shown off too: an update that stops halfway leaves it so) */
    ui.redraw = 1;
}

static uint32_t text_len(const char *s)
{
    uint32_t n = 0;
    while (s[n])
        n++;
    return n;
}

static void text_cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
}

static void int_text(int32_t v, char *b)     /* signed, no padding */
{
    char t[12];
    int n = 0;
    uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do
        t[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) && n < 11);
    if (v < 0)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
}

/* a rhythm's name; a FREE place's with its number among the mode's */
static const char *rhythm_name(uint32_t k)
{
    static char t[12];
    uint32_t i, n = 0;
    if (!RDEF[k].free)
        return RDEF[k].name;
    for (i = 0; i <= k; i++)
        if (RDEF[i].free && rhy_mode(i) == rhy_mode(k))
            n++;
    t[0] = 0;
    text_cat(t, "FREE ");
    int_text((int32_t)n, t + text_len(t));
    return t;
}

static void meter_text(const rhythm_t *r, char *t)   /* "4/4" */
{
    int_text((int32_t)meter_top(r), t);
    text_cat(t, meter_low(r) == 8u ? "/8" : "/4");
}

void app_say(const char *a, const char *b)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 2000u;
    ui.msg_rgb = 0;
    ui.msg_title = 0;
    ui.redraw = 1;
    if (ann_on()) {                          /* and to a screen reader (announce.c, fm1-reader) */
        t[0] = 0;
        text_cat(t, ui.msg[0]);
        if (ui.msg[1][0]) {
            text_cat(t, " ");
            text_cat(t, ui.msg[1]);
        }
        ann_say(t, 0, ANN_NOW);
    }
}

static void say_inst(const char *a, const char *b) { app_say(a, b); }   /* (an instrument's: its word in the accent) */

static void drone_words(char *t);

/* a screen reader that has just started listening (or asks) hears the whole screen */
static void speak(void)
{
    char t[128], n[12];
    if (ann_on() && ann_where() && drone()) {
        drone_words(t);
        text_cat(t, drone_on() ? " PLAYING. " : ". ");
        text_cat(t, PAGE_NAME[set.page]);
        ann_say(t, 0, ANN_NOW);
    } else if (ann_on() && ann_where()) {
        t[0] = 0;
        text_cat(t, shown()->name);
        text_cat(t, ". ");
        text_cat(t, rhythm_name(set.rhythm));
        text_cat(t, ", ");
        int_text(cur()->beats, n);
        text_cat(t, n);
        text_cat(t, bpm_per(cur()) == 2u ? " COUNTS, " : " BEATS, ");
        int_text((int32_t)bpm_shown(cur()), n);
        text_cat(t, n);
        text_cat(t, " BPM. ");
        text_cat(t, seq.rec ? "RECORDING. " : seq.playing ? "PLAYING. " : "");
        text_cat(t, PAGE_NAME[set.page]);
        ann_say(t, 0, ANN_NOW);
    }
    ann_tick(plat_ms());
}

/* ---------------------------------------------------------------- changes --- */
static void drone_apply(void);
static void changed(void)
{
    set_dirty = 1;
    ui.redraw = 1;
}

static void sound_apply(uint32_t i)      /* (the medio's and the viola's pitch over the gunga's strokes) */
{
    dr_inst_t p = *snd_of(i);
    p.tune += (float)INST[i].pitch;
    dr_inst(i, &p);
}

static void inst_step(int32_t d)
{
    char t[24];
    uint32_t m = berimbau(), n = ROW_N[m], i = 0;
    while (i < n && ROW_ORDER[m][i] != set.inst)
        i++;
    set.inst = ROW_ORDER[m][(i + (d > 0 ? 1u : n - 1u)) % n];   /* (in SELECT's order) */
    ui.clear_t = 0;
    ui.msg_until = 0;                    /* the footer says it, in its colour; the rhythm's name at the top, no message */
    changed();
    if (ann_on()) {                      /* a screen reader hears it */
        t[0] = 0;
        text_cat(t, shown()->name);
        if (cur()->mute >> set.inst & 1u)
            text_cat(t, " MUTED");
        ann_say(t, 0, ANN_NOW);
    }
}

static void keypage_fit(void)
{
    if (ui.keypage >= npages(cur()))
        ui.keypage = (uint8_t)(npages(cur()) - 1u);
}

static void div_step(int32_t d)
{
    rhythm_t *r = cur();
    char t[16];
    int32_t v = clamp((int32_t)r->div + d, 2, 4);
    if (v == r->div)
        return;
    r->div = (uint8_t)v;
    keypage_fit();
    changed();
    int_text(v, t);
    text_cat(t, " A BEAT");
    app_say("STEPS", t);
}

static int has_strokes(const rhythm_t *r, uint32_t i)
{
    uint32_t s;
    for (s = 0; s < nsteps(r); s++)
        if (ART(r->step[i][s]))
            return 1;
    return 0;
}

#define RHY_SAY_MS 4000u                 /* a rhythm chosen: its beats and name stay this long */

static int any_strokes(const rhythm_t *r)
{
    uint32_t i;
    for (i = 0; i < NROW; i++)
        if (has_strokes(r, i))
            return 1;
    return 0;
}

/* the rhythm come in, on the screen: its first keys' page, an instrument it plays shown */
static void rhythm_shown_fit(void)
{
    uint32_t i;
    ui.keypage = 0;
    if (any_strokes(cur()) && !has_strokes(cur(), set.inst))
        for (i = 0; i < ROW_N[berimbau()]; i++)   /* (in SELECT's order) */
            if (has_strokes(cur(), ROW_ORDER[berimbau()][i])) {
                set.inst = ROW_ORDER[berimbau()][i];
                break;
            }
    changed();
}

/* a rhythm chosen: a written one comes as it is written (its steps, swing and strokes: what was changed in it is
 * gone; its tempo as it was last set); a FREE one as the player left it. Playing, it comes in on the next bar's
 * first step (the one playing chosen again: it stays). Said: its name (its beats are in the footer), and NEXT while
 * it waits for its bar */
static void rhythm_select(uint32_t k)
{
    uint32_t i;
    if (seq.playing && !seq.count && seq.step >= 0) {
        seq.next = 0;
        if (k != set.rhythm) {
            if (!RDEF[k].free)
                rhythm_default(k);
            seq.next = (uint8_t)(k + 1u);
        }
        app_say(rhythm_name(k), k == set.rhythm ? "" : "NEXT");
    } else {
        if (!RDEF[k].free)
            rhythm_default(k);
        for (i = 0; i < NROW; i++)       /* (a stroke recorded ahead was the old rhythm's) */
            seq.skip[i] = 0;
        set.rhythm = (uint8_t)k;
        seq.jump = 1;
        rhythm_shown_fit();
        app_say(rhythm_name(k), "");
    }
    ui.redraw = 1;
    ui.msg_until = plat_ms() + RHY_SAY_MS;   /* time to read it while turning on */
    ui.msg_title = 1;
}

/* what PRESET and the list show: the written rhythms and the FREE places that hold one (only SAVE's list shows an empty
 * place) */
static int rhythm_listed(uint32_t k) { return !RDEF[k].free || any_strokes(&rhy[k]); }

/* the rhythms in their order on the FM-1 (PRESET, the list): the kit's then the berimbau's, by their beats, each
 * group's FREE places last, the empty ones after all (its places in rhy are the store's: one added later keeps the
 * older ones') */
static uint8_t ORD[NRHY], POS[NRHY];

static uint32_t ord_key(uint32_t k)
{
    return rhy_mode(k) << 17 | (uint32_t)!rhythm_listed(k) << 16 | (uint32_t)rhy[k].beats << 12 | RDEF[k].free << 8 | k;
}

static void ord_init(void)
{
    uint32_t i, j;
    for (i = 0; i < NRHY; i++) {
        for (j = i; j > 0u && ord_key(ORD[j - 1u]) > ord_key(i); j--)
            ORD[j] = ORD[j - 1u];
        ORD[j] = (uint8_t)i;
    }
    for (i = 0; i < NRHY; i++)
        POS[ORD[i]] = (uint8_t)i;
}

/* PRESET: the next rhythm with the same beats (round): the kit's, or the berimbau's toques */
static void rhythm_step(int32_t d)
{
    uint32_t k0 = rhythm_shown(), p, k = k0, n;
    ord_init();
    p = POS[k0];
    for (n = 0; n < NRHY; n++) {
        p = (p + (d > 0 ? 1u : NRHY - 1u)) % NRHY;
        k = ORD[p];
        if (rhy[k].beats == rhy[k0].beats && rhy_mode(k) == rhy_mode(k0) && rhythm_listed(k))
            break;
    }
    if (k != k0)
        rhythm_select(k);
}

/* ALGORITHM: the beats, 2 to 8: the first rhythm with that many (the berimbau's: the next beats it has a toque in) */
static void meter_step(int32_t d)
{
    int32_t b = rhy[rhythm_shown()].beats;
    uint32_t i;
    ord_init();
    while ((b += d) >= 2 && b <= 8)
        for (i = 0; i < NRHY; i++) {
            uint32_t k = ORD[i];
            if (rhy[k].beats == (uint32_t)b && rhy_mode(k) == rhy_mode(set.rhythm) && rhythm_listed(k)) {
                rhythm_select(k);
                return;
            }
        }
}

static void bpm_set(int32_t v)
{
    char t[12];
    ui.rise = 0;                         /* (a tempo set: it stays there) */
    cur()->bpm = (uint8_t)clamp(v, 40, 240);
    set.bpm[set.rhythm] = cur()->bpm;    /* (the rhythm comes back at it) */
    changed();
    if (!set.sh_tempo)                   /* the shruti's bellows at the rhythm's tempo */
        drone_apply();
    int_text((int32_t)bpm_shown(cur()), t);
    text_cat(t, " BPM");
    app_say("TEMPO", t);
}

static void pct_step(uint8_t *p, int32_t d, const char *name)
{
    char t[8];
    *p = (uint8_t)clamp((int32_t)*p + 5 * d, 0, 100);
    changed();
    int_text(*p, t);
    text_cat(t, "%");
    app_say(name, t);
}

/* SEQ: the tempo from the last taps (two at least, less than 1.5 s apart) */
static void tap(void)
{
    uint32_t now = plat_ms(), i;
    if (ui.ntap && now - ui.tap[ui.ntap - 1u] > 1500u)
        ui.ntap = 0;
    if (ui.ntap == 4u) {
        for (i = 0; i < 3u; i++)
            ui.tap[i] = ui.tap[i + 1u];
        ui.ntap = 3;
    }
    ui.tap[ui.ntap++] = now;
    if (ui.ntap >= 2u && ui.tap[ui.ntap - 1u] - ui.tap[0] >= 100u) {
        seq.jump = 1;                    /* tapped: there at once */
        bpm_set((int32_t)(60000u * (ui.ntap - 1u) + (ui.tap[ui.ntap - 1u] - ui.tap[0]) / 2u) /
                (int32_t)(ui.tap[ui.ntap - 1u] - ui.tap[0]) * (int32_t)bpm_per(cur()));
    } else
        app_say("TAP", "AGAIN");
}

/* -------------------------------------------------------------- the knobs --- */
enum { KN_BPM, KN_DIV, KN_SWING, KN_HUMAN, KN_LEVEL, KN_TUNE, KN_DECAY, KN_WIRES, KN_BEND, KN_CLICK, KN_COUNT,
       KN_LIGHT, KN_VEL, KN_CABACA, KN_VIRADA, KN_MARACAS, KN_NONE };
static const uint8_t PAGE_KNOB[NPAGES][4] = {
    {KN_BPM, KN_DIV, KN_SWING, KN_HUMAN},
    {KN_NONE, KN_NONE, KN_NONE, KN_NONE},   /* SOUND: the instrument's own (SOUND_KNOB) */
    {KN_CLICK, KN_COUNT, KN_LIGHT, KN_VEL},
};
/* SOUND: what a player of each can change. The djembe's head is tuned by its ropes and muted by a hand; the cajon's
 * snare wires are set tighter or slacker (or off), and a heel on its front deadens it; an udu comes smaller or larger,
 * and the palm over its opening lowers its bass; the maracas shake short or long, and are of one kind or another
 * (TYPE: a shaker, a classic pair, a gourd, the Daime's); the triangle is let ring or held;
 * a berimbau's wire is tuned tighter or slacker, its gourd closed on the belly (CABACA: how far a key pressed all the
 * way closes it), and it varies its toque now and then (VIRADAS); the atabaque's head is tuned; the clapsticks, the
 * caxixi, the agogo, the pandeiro and the reco-reco only play louder or softer */
static const uint8_t SOUND_KNOB[NINST][4] = {
    {KN_LEVEL, KN_TUNE, KN_DECAY, KN_NONE},
    {KN_LEVEL, KN_WIRES, KN_DECAY, KN_NONE},
    {KN_LEVEL, KN_TUNE, KN_BEND, KN_NONE},
    {KN_LEVEL, KN_NONE, KN_NONE, KN_NONE},
    {KN_LEVEL, KN_DECAY, KN_MARACAS, KN_NONE},
    {KN_LEVEL, KN_DECAY, KN_NONE, KN_NONE},
    {KN_LEVEL, KN_TUNE, KN_CABACA, KN_VIRADA},
    {KN_LEVEL, KN_NONE, KN_NONE, KN_NONE},
    {KN_LEVEL, KN_NONE, KN_NONE, KN_NONE},
    {KN_LEVEL, KN_TUNE, KN_CABACA, KN_VIRADA},
    {KN_LEVEL, KN_TUNE, KN_CABACA, KN_VIRADA},
    {KN_LEVEL, KN_NONE, KN_NONE, KN_NONE},
    {KN_LEVEL, KN_TUNE, KN_NONE, KN_NONE},
    {KN_LEVEL, KN_NONE, KN_NONE, KN_NONE},
};
static const char *const KNOB_NAME[KN_NONE] = {"TEMPO", "STEPS", "SWING", "HUMAN", "LEVEL", "TUNE", "DECAY",
                                               "SNARES", "BEND", "CLICK", "COUNT-IN", "LIGHTS", "VELOCITY",
                                               "CABACA", "VIRADAS", "TYPE"};
static const char *const VIRADA_NAME[5] = {"OFF", "EVERY 8", "EVERY 4", "EVERY 2", "EVERY BAR"};
static const char *const MARACAS_NAME[NMARACAS] = {"SHAKER", "CLASSIC", "GOURD", "DAIME"};
#define TUNE_MAX 3.0f                    /* semitones either way */

static uint32_t page_knob(uint32_t page, uint32_t k)
{
    return page == PG_SOUND ? SOUND_KNOB[sinst()][k] : PAGE_KNOB[page][k];
}

static void knob_text(uint32_t k, char *t)
{
    dr_inst_t *s = snd_of(sinst());
    int32_t v;
    t[0] = 0;
    switch (k) {
    case KN_BPM: int_text((int32_t)bpm_shown(cur()), t); break;
    case KN_DIV: int_text(cur()->div, t); text_cat(t, " A BEAT"); break;
    case KN_SWING: int_text(cur()->swing, t); text_cat(t, "%"); break;
    case KN_HUMAN: int_text(cur()->human, t); text_cat(t, "%"); break;
    case KN_LEVEL: int_text((int32_t)(s->level * 100.0f + 0.5f), t); text_cat(t, "%"); break;
    case KN_TUNE:                        /* in half steps: "+1.5" */
        v = (int32_t)(s->tune * 2.0f + (s->tune < 0.0f ? -0.5f : 0.5f));
        if (v > 0)
            text_cat(t, "+");
        else if (v < 0)
            text_cat(t, "-");
        int_text((v < 0 ? -v : v) / 2, t + (v != 0));
        if (v % 2)
            text_cat(t, ".5");
        break;
    case KN_DECAY: int_text((int32_t)(s->decay * 100.0f + 0.5f), t); text_cat(t, "%"); break;
    case KN_WIRES:
        if (s->wires < 0.005f)
            text_cat(t, "OFF");
        else {
            int_text((int32_t)(s->wires * 100.0f + 0.5f), t);
            text_cat(t, "%");
        }
        break;
    case KN_BEND:
        if (set.bend) {
            t[0] = '-';
            int_text(set.bend, t + 1);
        } else
            text_cat(t, "OFF");
        break;
    case KN_CLICK: text_cat(t, set.click == CLICK_OFF ? "OFF" : set.click == CLICK_REC ? "REC" : "ALWAYS"); break;
    case KN_COUNT: text_cat(t, set.countin ? "1 BAR" : "OFF"); break;
    case KN_LIGHT: text_cat(t, set.lights == 1u ? "LOW" : "HIGH"); break;
    case KN_VEL: text_cat(t, set.fixed ? "FIXED" : "KEYS"); break;
    case KN_CABACA:
        if (set.cabaca) {
            int_text(set.cabaca, t);
            text_cat(t, "%");
        } else
            text_cat(t, "OFF");
        break;
    case KN_VIRADA: text_cat(t, ber_idx(sinst()) < 0 ? "" : VIRADA_NAME[set.virada[ber_idx(sinst())]]); break;
    case KN_MARACAS: text_cat(t, MARACAS_NAME[set.maracas]); break;
    default: break;
    }
}

static int32_t knob_fill(uint32_t k, int32_t w)   /* a value's bar, 0..w; -1: no bar */
{
    const dr_inst_t *s = snd_of(sinst());
    float f;
    switch (k) {
    case KN_BPM: f = (float)(cur()->bpm - 40) / 200.0f; break;
    case KN_SWING: f = (float)cur()->swing * 0.01f; break;
    case KN_HUMAN: f = (float)cur()->human * 0.01f; break;
    case KN_LEVEL: f = s->level; break;
    case KN_TUNE: f = (s->tune + TUNE_MAX) / (2.0f * TUNE_MAX); break;
    case KN_DECAY: f = (s->decay - 0.1f) / 0.9f; break;
    case KN_WIRES: f = s->wires; break;
    case KN_BEND: f = (float)set.bend / 6.0f; break;
    case KN_CABACA: f = (float)set.cabaca * 0.01f; break;
    default: return -1;
    }
    return (int32_t)(fm_clampf(f, 0.0f, 1.0f) * (float)w + 0.5f);
}

static void knob_turn(uint32_t k, int32_t d)
{
    dr_inst_t *s = snd_of(sinst());
    char t[16];
    switch (k) {
    case KN_BPM: bpm_set((int32_t)cur()->bpm + d * (int32_t)bpm_per(cur())); return;
    case KN_DIV: div_step(d > 0 ? 1 : -1); return;
    case KN_SWING:
        if (cur()->div == 3u) {          /* 3 steps a beat: the swing is already there */
            app_say("SWING", "NONE IN 3");
            return;
        }
        pct_step(&cur()->swing, d, "SWING");
        return;
    case KN_HUMAN: pct_step(&cur()->human, d, "HUMAN"); return;
    case KN_LEVEL: s->level = fm_clampf(s->level + 0.05f * (float)d, 0.0f, 1.0f); break;
    case KN_TUNE: s->tune = fm_clampf(s->tune + 0.5f * (float)d, -TUNE_MAX, TUNE_MAX); break;
    case KN_DECAY: s->decay = fm_clampf(s->decay + 0.05f * (float)d, 0.1f, 1.0f); break;
    case KN_WIRES: s->wires = fm_clampf(s->wires + 0.05f * (float)d, 0.0f, 1.0f); break;
    case KN_BEND: set.bend = (uint8_t)clamp((int32_t)set.bend + d, 0, 6); break;
    case KN_CLICK: set.click = (uint8_t)clamp((int32_t)set.click + (d > 0 ? 1 : -1), CLICK_OFF, CLICK_ON); break;
    case KN_COUNT: set.countin ^= 1u; break;
    case KN_LIGHT: set.lights = (uint8_t)(set.lights == 1u ? 2u : 1u); break;
    case KN_VEL: set.fixed ^= 1u; break;
    case KN_CABACA: set.cabaca = (uint8_t)clamp((int32_t)set.cabaca + 5 * d, 0, 100); break;
    case KN_VIRADA:
        if (ber_idx(sinst()) >= 0)
            set.virada[ber_idx(sinst())] = (uint8_t)clamp((int32_t)set.virada[ber_idx(sinst())] + (d > 0 ? 1 : -1), 0, 4);
        break;
    case KN_MARACAS: set.maracas = (uint8_t)clamp((int32_t)set.maracas + (d > 0 ? 1 : -1), 0, NMARACAS - 1); break;
    default: return;
    }
    sound_apply(sinst());
    changed();
    knob_text(k, t);
    if (ann_on()) {                      /* the page shows the value: a screen reader hears it */
        char a[40] = "";
        text_cat(a, KNOB_NAME[k]);
        text_cat(a, " ");
        text_cat(a, t);
        ann_say(a, 0, ANN_NOW);
    }
}

/* ------------------------------------------------------------------- play --- */
static uint32_t key_vel(uint32_t k)
{
    uint32_t v = set.fixed ? 100u : touch_vel(k);
    return v ? v : 100u;
}

static void pad_stroke(uint32_t inst, uint32_t art, uint32_t vel, uint32_t delay)
{
    uint32_t k = padq.w % 16u;
    if (padq.w - padq.r >= 16u)
        return;
    padq.inst[k] = (uint8_t)inst;
    padq.art[k] = (uint8_t)art;
    padq.vel[k] = (uint8_t)vel;
    padq.delay[k] = (uint16_t)delay;
    __asm__ volatile("" ::: "memory");   /* the slot, then its publication */
    padq.w++;
}

static void pad_play(uint32_t inst, uint32_t art, uint32_t vel, uint32_t flam)
{
    if (flam)                            /* the grace stroke now, the stroke just after */
        pad_stroke(inst, art, (uint32_t)((float)vel * FLAM_GRACE), 0);
    pad_stroke(inst, art, vel, flam ? (uint32_t)(FLAM_S * SR) : 0u);
}

static int flam_held(void) { return (int)(plat_buttons() >> B_ARP & 1u); }

/* REC: a stroke onto the nearest step (the one sounding, or the next; the next is then not played again this time
 * round). During the count-in, only in its last half step (its last step has the count at 0, no step yet): onto the
 * first step */
static int32_t record(uint32_t row, uint32_t art, uint32_t vel, uint32_t flam)   /* its step; -1: not recorded */
{
    rhythm_t *r = cur();
    uint32_t f = plat_lock(), ns = nsteps(r), count = seq.count, target, ahead = 0;
    int32_t step = seq.step;
    float left = seq.left, len = seq.len;
    plat_unlock(f);
    if (!seq.rec || !seq.playing)
        return -1;
    if (count)
        return -1;
    if (step < 0) {
        if (left >= len * 0.5f)
            return -1;
        target = 0;
        ahead = 1;
    } else if (left < len * 0.5f) {
        target = (uint32_t)step + 1u >= ns ? 0u : (uint32_t)step + 1u;
        ahead = 1;
    } else {
        target = (uint32_t)step;
    }
    r->step[row][target] = STEP(art, vel, flam);
    if (ahead)
        seq.skip[row] = (uint8_t)(target + 1u);
    changed();
    return (int32_t)target;
}

/* a white key: its step's next stroke for the instrument (after the last: none); with ARP held, the djembe's step a
 * flam or not (an empty one its first stroke, a flam). Stopped, it sounds */
static void step_key(uint32_t w, uint32_t k)
{
    rhythm_t *r = cur();
    uint32_t s = ui.keypage * page_steps(r) + w, v, a, flam;
    if (w >= page_steps(r) || s >= nsteps(r))
        return;
    v = r->step[set.inst][s];
    if (sinst() == I_DJEMBE && flam_held()) {
        a = ART(v) ? ART(v) : 1u;
        flam = ART(v) ? !FLAM(v) : 1u;
    } else {
        a = ART(v) + 1u;
        if (a > shown()->nart)
            a = 0;
        flam = FLAM(v);
    }
    r->step[set.inst][s] = a ? STEP(a, key_vel(k), flam) : 0u;
    if (a && !seq.playing)
        pad_play(sinst(), a, key_vel(k), flam);
    ui.clear_t = 0;
    changed();
}

/* a black key: its row and stroke (0: none on it); the berimbau's first three, the berimbau shown; on the berimbau's
 * page with ARP, the fourth group's two the agogo's bells */
static uint32_t pad_row(uint32_t b, uint32_t *art)
{
    const uint8_t *p = PAD[berimbau()][b];
    *art = p[1];
    if (berimbau() && flam_held() && (b == AGOGO_PAD || b == AGOGO_PAD + 1u)) {
        *art = b - AGOGO_PAD + 1u;
        return RB_AGOGO;
    }
    if (p[0] == PAD_SHOWN)
        return ber_idx(sinst()) >= 0 ? set.inst : RB_GUNGA;
    return p[0];
}

static void ladainha_press(void);

static void pad_key(uint32_t b, uint32_t k)
{
    uint32_t art, row = pad_row(b, &art), in, vel = key_vel(k), flam;
    int32_t s;
    if (row == PAD_VIRADA && flam_held()) {
        ladainha_press();
        return;
    }
    if (row == PAD_VIRADA) {             /* the berimbau shown varies the next bar */
        seq.vreq = (uint8_t)((ber_idx(sinst()) >= 0 ? set.inst : RB_GUNGA) + 1u);
        say_inst("VIRADA", "NEXT BAR");
        return;
    }
    if (!art)                            /* no stroke on it */
        return;
    in = ROW_INST[berimbau()][row];
    flam = in == I_DJEMBE && flam_held();
    pad_play(in, art, vel, flam);
    s = record(row, art, vel, flam);
    if (!berimbau() && b == TRIANGLE_PAD) {   /* how long it is held tells open from muted */
        ui.tri_down = plat_ms();
        ui.tri_step = s;
        ui.tri_rhythm = set.rhythm;
    }
}

/* the triangle's key let go: the hand closes on it; let go at once, the stroke recorded is a muted one */
static void triangle_release(void)
{
    rhythm_t *r = cur();
    pad_stroke(I_TRIANGLE, 0, 0, 0);
    if (ui.tri_step >= 0 && ui.tri_rhythm == set.rhythm && plat_ms() - ui.tri_down < TAP_MS) {
        uint32_t v = r->step[I_TRIANGLE][ui.tri_step];
        if (ART(v) == 1u) {
            r->step[I_TRIANGLE][ui.tri_step] = STEP(2u, VEL(v), FLAM(v));
            changed();
        }
    }
    ui.tri_step = -1;
}

/* ------------------------------------------------------------------ input --- */
/* the store's rhythms: the FREE places as saved (what is changed on one since stays out, until SAVE's list puts it
 * there) */
static void keep_init(void)
{
    uint32_t k, n = 0;
    keep.magic = RHY_MAGIC;
    for (k = 0; k < NRHY; k++)
        if (RDEF[k].free) {
            keep.which[n] = (uint8_t)k;
            keep.r[n++] = rhy[k];
        }
    while (n < NKEEP) {
        keep.which[n] = 0xFFu;
        keep.r[n++] = rhy[0];
    }
}

static rhythm_t *kept(uint32_t k)        /* a FREE place's record */
{
    uint32_t i;
    for (i = 0; i < NKEEP; i++)
        if (keep.which[i] == k)
            return &keep.r[i];
    return 0;
}

/* a rhythm changed: from what is written, a FREE place's from what is saved. Its tempo (kept anyway) and what is muted
 * are no change */
static int rhythm_changed(uint32_t k)
{
    rhythm_t one, now = rhy[k];
    const rhythm_t *o = &one;
    uint32_t i;
    if (RDEF[k].free)
        o = kept(k);
    else
        rhythm_build(k, &one);
    if (!o)
        return 1;
    now.bpm = o->bpm;
    now.mute = o->mute;
    for (i = 0; i < sizeof now; i++)
        if (((const uint8_t *)&now)[i] != ((const uint8_t *)o)[i])
            return 1;
    return 0;
}

static int save_all(void)
{
    if (plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0 && plat_store_save(OBJ_RHYTHMS, &keep, sizeof keep) == 0) {
        set_dirty = 0;
        app_say("SAVED", "");
        return 1;
    }
    app_say("SAVE", "FAILED");
    return 0;
}

/* ------------------------------------------------------------ the drones --- */
static int32_t tanpura_sa(void) { return 48 + set.tp_sa + 12 * set.tp_high; }
static const int8_t FIRST_OFF[3] = {-5, -7, -1};   /* the first string under Sa: Pa, Ma, Ni */
static const char *const FIRST_NAME[3] = {"PA", "MA", "NI"};

/* PRESETS, the tanpura: its character, from the slowest: how fast its strings come round (the cycle) and how much
 * they buzz (the jawari, the thread under them: a closed one soft and round, an open one bright). MEDITATION slow
 * and soft; MANTRA round; RAGA the concert tanpura's open jawari; KIRTAN quicker and brighter. ALGORITHM tunes its
 * first string (Pa, Ma, Ni), a key sets Sa */
typedef struct {
    const char *name;
    uint8_t cycle, jawari;               /* tenths of a second; % */
} tp_preset_t;
static const tp_preset_t TP_PRESET[] = {
    {"MEDITATION", 70, 30}, {"MANTRA", 50, 55}, {"RAGA", 40, 75}, {"KIRTAN", 30, 90},
};
#define NTP (sizeof TP_PRESET / sizeof TP_PRESET[0])
/* PRESETS, the shruti: its reeds opened one by one (NOTES), or a chord on the key pressed: its root, fifth and octave
 * (SA PA), with the major or the minor third */
enum { SH_NOTES, SH_SAPA, SH_MAJOR, SH_MINOR, NSH };
static const char *const SH_NAME[NSH] = {"NOTES", "SA PA", "MAJOR", "MINOR"};
static const uint8_t SH_THIRD[NSH] = {0, 0, 4, 3};

static uint32_t chord(uint32_t root, uint32_t kind)   /* its reeds (above the top key: an octave down) */
{
    const uint8_t iv[4] = {0, 7, 12, SH_THIRD[kind]};
    uint32_t m = 0, i, k;
    for (i = 0; i < (SH_THIRD[kind] ? 4u : 3u); i++) {
        k = root + iv[i];
        m |= 1u << (k < NKEYS ? k : k - 12u);
    }
    return m;
}

static void shruti_name(char *t)         /* "A MINOR"; "NOTES" */
{
    t[0] = 0;
    if (set.sh_kind != SH_NOTES) {
        text_cat(t, NOTE_ABC[(53u + set.sh_root) % 12u]);
        text_cat(t, " ");
    }
    text_cat(t, SH_NAME[set.sh_kind]);
}

/* the shruti's bellows: their own tempo, or the rhythm's (0: RHYTHM; above 120 half of it) */
static uint32_t shruti_tempo(void)
{
    uint32_t b = set.sh_tempo ? set.sh_tempo : cur()->bpm;
    while (b > 120u)
        b /= 2u;
    return b;
}

static void drone_apply(void)
{
    dn_tanpura_t p;
    p.sa = tanpura_sa();
    p.first = set.tp_first;
    p.cycle_s = (float)set.tp_cycle * 0.1f;
    p.jawari = (float)set.tp_jawari * 0.01f;
    p.level = (float)set.tp_level * 0.01f;
    dn_tanpura(ui.drone == MODE_TANPURA, &p);
    dn_shruti(ui.drone == MODE_SHRUTI, set.sh_notes, 53, (float)set.sh_level * 0.01f, (float)set.sh_bellows * 0.01f,
              60.0f / (float)shruti_tempo());
}

/* the kit and the berimbau each keep their rhythm and instrument: the ones shown are the mode's */
static void rhythm_box_fit(void)
{
    uint8_t r = set.rhythm, i = set.inst;
    if (drone() || rhy_mode(set.rhythm) == set.mode)
        return;
    set.rhythm = set.other_rhythm;
    set.inst = set.other_inst;
    set.other_rhythm = r;
    set.other_inst = i;
    ui.keypage = 0;
}

static void battery_show(uint32_t ms)    /* the battery on the screen, a moment: 5 s after the opening, 3 s on HOME, FX */
{
    ui.batt_until = plat_ms() + ms;
    ui.redraw = 1;
}

/* the battery on the screen: that moment, or all the time while it is low (one bar left, or none) */
static int battery_shown(void) { return plat_ms() < ui.batt_until || ui.batt == 0 || ui.batt == 1; }

/* the mode shown: the kit, the berimbau, a drone. The rhythm playing goes on under a drone's screen, and a drone
 * under the kit's or the berimbau's; the kit and the berimbau (one sequencer) stop each other */
static void mode_show(uint32_t m)
{
    battery_show(3000u);
    set.mode = (uint8_t)m;
    if (!drone() && rhy_mode(set.rhythm) != set.mode)
        seq_stop();
    rhythm_box_fit();
    ui.list_on = 0;
    if (drone() && set.page == PG_SOUND)
        set.page = PG_HOME;              /* a drone's settings are on its own screen */
    ui.keypage = 0;
    ui.clear_t = 0;
    changed();
    app_say(MODE_NAME[set.mode], "");
    ui.msg_until = plat_ms() + RHY_SAY_MS;
    ui.msg_title = 1;
    ui.msg_rgb = MODE_RGB[set.mode];
}

/* FX: the next, the drones right after the kit (the kit playing on under them), then the berimbau */
static void mode_step(void)
{
    static const uint8_t NEXT[NMODES] = {MODE_TANPURA, MODE_KIT, MODE_SHRUTI, MODE_BERIMBAU};
    mode_show(NEXT[set.mode]);
}

static void solo_key(uint32_t k)
{
    if (set.mode == MODE_TANPURA)        /* a key: its note is Sa */
        set.tp_sa = (uint8_t)((53u + k) % 12u);
    else if (set.sh_kind == SH_NOTES)    /* a key: its reed opens or closes */
        set.sh_notes ^= 1u << k;
    else {                               /* a key: the chord on it */
        set.sh_root = (uint8_t)k;
        set.sh_notes = chord(k, set.sh_kind);
    }
    changed();
    drone_apply();
}

/* ALGORITHM, the tanpura: its first string's tuning, Pa, Ma or Ni (as the kit's beats, it goes no further) */
static void tanpura_first(int32_t d)
{
    int32_t f = clamp((int32_t)set.tp_first + d, 0, 2);
    if (f == set.tp_first)
        return;
    char t[8] = "SA ";
    set.tp_first = (uint8_t)f;
    changed();
    drone_apply();
    text_cat(t, FIRST_NAME[f]);          /* SA PA, SA MA, SA NI: as the tanpura is said to be tuned */
    app_say(t, "");
    ui.msg_until = plat_ms() + RHY_SAY_MS;
    ui.msg_title = 1;
}

/* a drone's screen in words (speak): its name, the tanpura's Sa, tuning and character, the shruti's chord */
static void drone_words(char *t)
{
    t[0] = 0;
    text_cat(t, MODE_NAME[set.mode]);
    text_cat(t, ". ");
    if (set.mode == MODE_TANPURA) {
        text_cat(t, "SA ");
        text_cat(t, NOTE_ABC[set.tp_sa]);
        text_cat(t, ", SA ");
        text_cat(t, FIRST_NAME[set.tp_first]);
        text_cat(t, ", ");
        text_cat(t, TP_PRESET[set.tp_preset].name);
    } else
        shruti_name(t + text_len(t));
}

/* a drone's knob turned: its setting to a screen reader (the screen shows it) */
static void solo_say(uint32_t i)
{
    char t[32], n[12];
    if (!ann_on())
        return;
    t[0] = n[0] = 0;
    if (set.mode == MODE_TANPURA) {
        static const char *const NAME[4] = {"JAWARI ", "CYCLE ", "REGISTER ", "LEVEL "};
        text_cat(t, NAME[i]);
        if (i == 1u) {
            int_text(set.tp_cycle / 10, n);
            text_cat(n, ".");
            int_text(set.tp_cycle % 10, n + text_len(n));
            text_cat(n, " S");
        } else if (i == 2u)
            text_cat(n, set.tp_high ? "HIGH" : "LOW");
        else {
            int_text(i ? set.tp_level : set.tp_jawari, n);
            text_cat(n, "%");
        }
    } else {
        static const char *const NAME[3] = {"BREATHING ", "TEMPO ", "LEVEL "};
        text_cat(t, NAME[i]);
        if (i == 1u && !set.sh_tempo)
            text_cat(t, "RHYTHM ");
        int_text(i == 0u ? set.sh_bellows : i == 1u ? (int32_t)shruti_tempo() : set.sh_level, n);
        text_cat(n, i == 1u ? " BPM" : "%");
    }
    text_cat(t, n);
    ann_say(t, 0, ANN_NOW);
}

/* PRESETS in a drone: the tanpura's character, the shruti's chord (on the key pressed last); its name large */
static void drone_preset(uint32_t i)
{
    char t[24];
    if (set.mode == MODE_TANPURA) {
        set.tp_preset = (uint8_t)i;
        set.tp_cycle = TP_PRESET[i].cycle;
        set.tp_jawari = TP_PRESET[i].jawari;
        app_say(TP_PRESET[i].name, "");
    } else {
        set.sh_kind = (uint8_t)i;
        if (i != SH_NOTES)
            set.sh_notes = chord(set.sh_root, i);
        shruti_name(t);
        app_say(t, "");
    }
    ui.msg_until = plat_ms() + RHY_SAY_MS;
    ui.msg_title = 1;
    changed();
    drone_apply();
}

/* -------------------------------------------------------- the presets' list --- */
/* LFO: the mode's presets listed (the kit's rhythms, the berimbau's toques, the tanpura's characters, the shruti's
 * chords), from the one chosen; the list stays, KNOB 3 goes through it (round), nothing chosen; LFO held chooses the
 * one shown, LFO again closes the list */
#define LONG_MS 500u                     /* a press held this long: a long one */

static uint32_t list_n(void)
{
    uint32_t k, n = 0;
    if (drone())
        return set.mode == MODE_TANPURA ? NTP : NSH;
    for (k = 0; k < NRHY; k++)
        if (rhy_mode(k) == set.mode && (ui.list_save ? RDEF[k].free : rhythm_listed(k)))
            n++;
    return n;
}

/* the list's rhythm i: in ORD (the ones shown come first); in SAVE's list, the mode's FREE places */
static uint32_t list_rhythm(uint32_t i)
{
    uint32_t k;
    if (!ui.list_save)
        return ORD[i + (berimbau() ? NRHY_KIT : 0u)];
    for (k = 0; k < NRHY; k++)
        if (RDEF[k].free && rhy_mode(k) == set.mode) {
            if (!i)
                return k;
            i--;
        }
    return 0;
}

static uint32_t list_now(void)           /* the one chosen (none: past the last) */
{
    uint32_t i, n;
    if (drone())
        return set.mode == MODE_TANPURA ? set.tp_preset : set.sh_kind;
    if (!ui.list_save)
        return POS[rhythm_shown()] - (berimbau() ? NRHY_KIT : 0u);
    n = list_n();
    for (i = 0; i < n && list_rhythm(i) != set.rhythm; i++)
        ;
    return i;
}

static const char *list_name(uint32_t i)
{
    return set.mode == MODE_TANPURA ? TP_PRESET[i].name : set.mode == MODE_SHRUTI ? SH_NAME[i]
           : rhythm_name(list_rhythm(i));
}

/* a row's value, at the right: a rhythm's beats; in SAVE's list, what the place holds */
static void list_value(uint32_t i, char *t)
{
    const rhythm_t *r;
    t[0] = 0;
    if (drone())
        return;
    r = &rhy[list_rhythm(i)];
    if (!ui.list_save)
        int_text(r->beats, t);
    else if (any_strokes(r))
        meter_text(r, t);
    else
        text_cat(t, "EMPTY");
}

static int list_shown(void) { return ui.list_on; }
static int save_list(void) { return ui.list_on && ui.list_save; }

static void list_show(uint32_t i)        /* the list on the screen, i in the middle */
{
    ui.list_i = (uint8_t)i;
    ui.list_on = 1;
    ui.msg_until = 0;
    ui.redraw = 1;
    if (ann_on()) {                      /* a screen reader hears it: its name, its beats */
        char t[40] = "", n[8];
        text_cat(t, list_name(ui.list_i));
        list_value(ui.list_i, n);
        if (n[0]) {
            text_cat(t, ", ");
            text_cat(t, n);
            if (!ui.list_save)
                text_cat(t, bpm_per(cur()) == 2u ? " COUNTS" : " BEATS");
        }
        ann_say(t, 0, ANN_NOW);
    }
}

static void list_scroll(int32_t d)        /* KNOB 3, the list shown */
{
    list_show((ui.list_i + (uint32_t)((int32_t)list_n() * 8 + d)) % list_n());
}

static void list_choose(void)
{
    uint32_t i = ui.list_i;
    ui.list_on = 0;
    if (drone())
        drone_preset(i);
    else
        rhythm_select(list_rhythm(i));
}

/* SAVE: the settings, then, a rhythm changed, the list of the mode's FREE places, to choose where it goes (the place
 * it is on, or the first empty one, shown first); KNOB 3 goes through it, SAVE again puts it there, a page button
 * leaves it */
static void save_press(void)
{
    int changed_now = rhythm_changed(set.rhythm);
    uint32_t i, n;
    if (!save_all() || !changed_now)
        return;
    set_dirty = 1;                       /* (the rhythm not saved yet: SAVE stays lit) */
    ui.list_save = 1;
    n = list_n();
    if ((i = list_now()) == n)
        for (i = 0; i < n && any_strokes(&rhy[list_rhythm(i)]); i++)
            ;
    list_show(i < n ? i : 0);
}

/* the rhythm put in a FREE place, over what it holds: it plays on from there, nothing heard; what it came from is back
 * as it was */
static void slot_save(uint32_t k)
{
    uint32_t old = set.rhythm, f = plat_lock();
    rhy[k] = rhy[old];
    set.rhythm = (uint8_t)k;
    plat_unlock(f);
    *kept(k) = rhy[k];
    set.bpm[k] = rhy[k].bpm;
    if (k != old) {
        if (RDEF[old].free)
            rhy[old] = *kept(old);
        else
            rhythm_default(old);
    }
    ord_init();
    ui.list_on = 0;
    ui.redraw = 1;
    if (save_all())
        app_say("SAVED", rhythm_name(k));
}

/* SEQ held, playing: the tempo rises, as the lead of a jembe piece raises it, nearly evenly: a BPM every 12 beats
 * (R. Polak, Music Theory Online 16.4, 2010: 135 to 205 in 218 cycles of 4 beats). OCT or SEQ held again keep the tempo
 * reached, KNOB 1 or tapping end it at theirs; stopped, or another rhythm come in, the rhythm is back at its own */
static void rise_toggle(void)
{
    ui.ntap = 0;                         /* (its press was no tap) */
    if (ui.rise)
        bpm_set(cur()->bpm);
    else if (!seq.playing || seq.count)
        app_say("PLAY", "FIRST");
    else {
        ui.rise = 1;
        ui.rise_rhythm = set.rhythm;
        ui.rise_from = cur()->bpm;
        ui.rise_beats = seq.beats;
        app_say("TEMPO", "RISING");
    }
}

static void rise_poll(void)
{
    if (!ui.rise)
        return;
    if (!seq.playing || set.rhythm != ui.rise_rhythm) {
        rhy[ui.rise_rhythm].bpm = ui.rise_from;
        ui.rise = 0;
        ui.redraw = 1;
    } else if (seq.beats - ui.rise_beats >= 12u * bpm_per(cur())) {
        ui.rise_beats += 12u * bpm_per(cur());
        if ((uint32_t)cur()->bpm + bpm_per(cur()) <= 240u) {
            cur()->bpm = (uint8_t)(cur()->bpm + bpm_per(cur()));
            ui.redraw = 1;
            if (!set.sh_tempo)
                drone_apply();
        }
    }
}

/* ARP + VIRADA: the ladainha, RODA ANGOLA begun by the gunga (from a stop too); again: the chula, the rest of the bateria
 * on its bar; again: back to the ladainha's base */
static void ladainha_press(void)
{
    if (seq.playing && seq.lad == 1u) {
        seq.lad = 2;
        app_say("CHULA", "NEXT BAR");
    } else if (seq.playing && seq.lad == 2u) {
        seq.lad_step = 4;
        seq.lad = 1;
        app_say("LADAINHA", "NEXT BAR");
    } else {
        if (set.rhythm != R_RODA)
            rhythm_select(R_RODA);
        if (!seq.playing)
            seq_start(0);
        seq.lad_rows = 0;
        seq.lad_step = seq.lad_bar = 0;
        seq.lad = 1;
        app_say("LADAINHA", "");
    }
    ui.redraw = 1;
}

/* OCT- / OCT+: the tempo, 5 BPM a press, 10 held (rising: it keeps the tempo reached); LFO: the list shown or closed,
 * held: the one shown chosen; SEQ held: the tempo rising */
static void hold_action(uint32_t b, int held)
{
    if (b == B_SEQ) {
        if (held && !drone())
            rise_toggle();
    } else if (b == B_LFO) {
        if (held) {
            if (list_shown() && !ui.list_save)
                list_choose();
        } else if (list_shown()) {
            ui.list_on = 0;
            ui.redraw = 1;
        } else {
            uint32_t i;
            ui.list_save = 0;
            ord_init();
            i = list_now();
            list_show(i < list_n() ? i : 0);
        }
    } else if (!drone())
        bpm_set(ui.rise ? (int32_t)cur()->bpm
                        : (int32_t)cur()->bpm + (b == B_OCTUP ? 1 : -1) * (held ? 10 : 5) * (int32_t)bpm_per(cur()));
}

/* those act when let go, or once held long enough (SEQ, tapping on its press, only when held) */
static void hold_buttons(uint32_t btn)
{
    static const uint8_t HB[4] = {B_OCTDN, B_OCTUP, B_LFO, B_SEQ};
    uint32_t i, now = plat_ms();
    for (i = 0; i < 4u; i++) {
        uint32_t b = HB[i], on = btn >> b & 1u, was = ui.btn >> b & 1u;
        if (on && !was) {
            ui.down_at[i] = now;
            ui.held_long &= (uint8_t)~(1u << i);
        } else if (on && !(ui.held_long >> i & 1u) && now - ui.down_at[i] >= LONG_MS) {
            ui.held_long |= (uint8_t)(1u << i);
            hold_action(b, 1);
        } else if (!on && was && !(ui.held_long >> i & 1u))
            hold_action(b, 0);
    }
}

static void solo_knob(uint32_t i, int32_t d)
{
    if (set.mode == MODE_TANPURA) {      /* KNOB 1 its jawari, 2 its cycle, 3 its register, 4 its level */
        if (i == 0)
            set.tp_jawari = (uint8_t)clamp((int32_t)set.tp_jawari + 5 * d, 0, 100);
        else if (i == 1)
            set.tp_cycle = (uint8_t)clamp((int32_t)set.tp_cycle + 2 * d, 20, 80);
        else if (i == 2)
            set.tp_high ^= 1u;
        else
            set.tp_level = (uint8_t)clamp((int32_t)set.tp_level + 5 * d, 0, 100);
    } else if (i == 0)                   /* the shruti: KNOB 1 its breathing, KNOB 2 its bellows' tempo, 3 its level */
        set.sh_bellows = (uint8_t)clamp((int32_t)set.sh_bellows + 5 * d, 0, 100);
    else if (i == 1)
        set.sh_tempo = (uint8_t)(set.sh_tempo ? ((int32_t)set.sh_tempo + d < 20 ? 0 : clamp((int32_t)set.sh_tempo + d, 20, 120))
                                 : d > 0 ? 20 : 0);   /* under 20: RHYTHM, the rhythm's tempo */
    else if (i == 2)
        set.sh_level = (uint8_t)clamp((int32_t)set.sh_level + 5 * d, 0, 100);
    else
        return;
    changed();
    drone_apply();
    solo_say(i);
}

/* PLAY, the rhythm playing: it stops on the next bar's first step; again, now */
static void play_stop(void)
{
    if (seq.stop_at || seq.count || seq.step < 0)
        seq_stop();
    else {
        seq.stop_at = 1;
        app_say("STOP", "ON THE 1");
    }
}

static void solo_button(uint32_t b)
{
    switch (b) {
    case B_FX:
        mode_step();
        break;
    case B_PLAY:                         /* the drone shown sounding: off; else the rhythm playing stops; else this
                                          * drone on (the other stopped) */
        if (!drone_on() && seq.playing)
            play_stop();
        else {
            ui.drone = (uint8_t)(drone_on() ? 0u : set.mode);
            drone_apply();
        }
        ui.redraw = 1;
        break;
    case B_SAVE:
        save_all();
        break;
    case B_HOME:                         /* on its own page already: back to the rhythm, the drone sounding on */
        if (set.page == PG_HOME && !list_shown()) {
            mode_show(rhy_mode(set.rhythm));
            break;
        }
        battery_show(3000u);
        set.page = PG_HOME;
        ui.list_on = 0;
        ui.redraw = 1;
        break;
    case B_GLO:
        set.page = PG_SETUP;
        ui.list_on = 0;
        ui.redraw = 1;
        break;
    case B_EDIT:                         /* the shruti: twice, every reed closed */
        if (set.mode != MODE_SHRUTI)
            break;
        if (ui.clear_t && plat_ms() - ui.clear_t < 2000u) {
            set.sh_notes = 0;
            ui.clear_t = 0;
            changed();
            drone_apply();
            app_say("NOTES", "CLEARED");
        } else {
            ui.clear_t = plat_ms();
            app_say("EDIT AGAIN", "TO CLEAR");
        }
        break;
    default:
        break;
    }
}

/* a drone's input: the keys, the buttons it uses, KNOB 1-4; its lights */
static void solo_input(uint32_t keys, uint32_t btn)
{
    uint32_t k, lit_keys = 0, dim_keys = 0, lit_btn, dim_btn, used, now = plat_ms();
    int32_t e;
    for (k = 0; k < NKEYS; k++)
        if ((keys >> k & 1u) && !(ui.keys >> k & 1u))
            solo_key(k);
    for (k = 0; k < NB; k++)
        if ((btn >> k & 1u) && !(ui.btn >> k & 1u))
            solo_button(k);
    for (k = 0; k < NE; k++) {           /* (SELECT: nothing here; ALGORITHM: the shruti nothing) */
        if ((e = plat_enc((int)k)) == 0)
            continue;
        if (k == EN_PRESET)
            drone_preset((list_now() + (e > 0 ? 1u : list_n() - 1u)) % list_n());
        else if (k == EN_ALGO && set.mode == MODE_TANPURA)
            tanpura_first(e > 0 ? 1 : -1);
        else if (k == EN_K3 && list_shown())
            list_scroll(e);
        else if (k >= EN_K1 && set.page == PG_SETUP)   /* SETUP shown: its knobs */
            knob_turn(PAGE_KNOB[PG_SETUP][k - EN_K1], e);
        else if (k >= EN_K1)
            solo_knob(k - EN_K1, e);
    }
    hold_buttons(btn);
    if (keys != ui.keys)
        ui.redraw = 1;
    ui.keys = keys;
    ui.btn = btn;
    if (set.mode == MODE_TANPURA) {      /* Sa's keys lit, the first string's dim */
        uint32_t first = (uint32_t)(set.tp_sa + 12 + FIRST_OFF[set.tp_first]) % 12u;
        for (k = 0; k < NKEYS; k++)
            if ((53u + k) % 12u == set.tp_sa)
                lit_keys |= 1u << k;
            else if ((53u + k) % 12u == first)
                dim_keys |= 1u << k;
        used = 1u << B_FX | 1u << B_PLAY | 1u << B_SAVE | 1u << B_HOME | 1u << B_GLO | 1u << B_LFO;
    } else {                             /* the reeds open lit, the others dim */
        lit_keys = set.sh_notes;
        dim_keys = ((1u << NKEYS) - 1u) & ~lit_keys;
        used = 1u << B_FX | 1u << B_PLAY | 1u << B_SAVE | 1u << B_HOME | 1u << B_GLO | 1u << B_EDIT | 1u << B_LFO;
    }
    lit_keys |= keys;
    lit_btn = (btn & used & ~(1u << B_PLAY)) | 1u << B_FX | 1u << (set.page == PG_SETUP ? B_GLO : B_HOME)
              | (set_dirty ? 1u << B_SAVE : 0u);
    if (drone_on() || (seq.playing && (!seq.stop_at || (now / 150u) & 1u)))
        lit_btn |= LED_PLAY_GREEN;       /* (blinking: the rhythm stops on the bar's first step) */
    if (ui.clear_t && now - ui.clear_t < 2000u)
        lit_btn |= 1u << B_EDIT;
    if (list_shown())
        lit_btn |= 1u << B_LFO;
    dim_btn = ~lit_btn & used & ~(1u << B_PLAY);
    if (!(lit_btn & LED_PLAY_GREEN))
        dim_btn |= LED_PLAY_GREEN;
    ui.glow = dim_btn;
    plat_lights(lit_btn, lit_keys, dim_btn, dim_keys & ~lit_keys, set.lights);
    plat_lights_mid(0, 0);
}

static void button(uint32_t b)
{
    uint32_t i;
    rhythm_t *r = cur();
    for (i = 0; i < NPAGES; i++)
        if (b == PAGE_BTN[i]) {          /* (the presets' list closed: the page shown) */
            if (b == B_HOME)
                battery_show(3000u);
            set.page = (uint8_t)i;
            ui.list_on = 0;
            ui.redraw = 1;
            return;
        }
    switch (b) {
    case B_PLAY:
        if (!seq.playing)
            seq_start(0);
        else
            play_stop();
        ui.redraw = 1;
        break;
    case B_REC:
        if (seq.rec)
            seq.rec = 0;
        else {
            if (!seq.playing)
                seq_start(set.countin);
            seq.rec = 1;
        }
        ui.redraw = 1;
        break;
    case B_SEL:
        r->mute ^= (uint8_t)(1u << set.inst);
        changed();
        say_inst(shown()->name, (r->mute >> set.inst & 1u) ? "MUTED" : "ON");
        break;
    case B_EDIT:
        if (ui.clear_t && plat_ms() - ui.clear_t < 2000u) {
            for (i = 0; i < NSTEPS; i++)
                r->step[set.inst][i] = 0;
            ui.clear_t = 0;
            changed();
            say_inst(shown()->name, "CLEARED");
        } else {
            ui.clear_t = plat_ms();
            say_inst("EDIT AGAIN", "TO CLEAR");
        }
        break;
    case B_SEQ:
        tap();
        break;
    case B_ARP:                          /* held: the djembe's flams; the berimbau's page: the agogo on the atabaque's keys,
                                          * the ladainha on VIRADA's */
        app_say("ARP", berimbau() ? "AGOGO, LADAINHA" : "FLAMS");
        break;
    case B_SAVE:
        if (save_list())
            slot_save(list_rhythm(ui.list_i));
        else
            save_press();
        break;
    case B_FX:
        mode_step();
        break;
    default:
        break;
    }
}

/* the battery: the keys' readings own the converter, and the platform reads the battery only when it is free; so
 * every 10 s (each second until a first reading), no key down and no computer powering the FM-1, the readings let it
 * go for 2 ms, about one reading a key */
static void battery_poll(uint32_t now)
{
    int b;
    if (ui.batt_off) {
        if (now - ui.batt_t >= 2u) {
            plat_sense(1);
            ui.batt_off = 0;
        }
    } else if (ui.batt != 4 && touch_is_on() && !(plat_keys() | touch_keys()) &&
               now - ui.batt_t >= (ui.batt < 0 ? 1000u : 10000u)) {
        plat_sense(0);
        ui.batt_off = 1;
        ui.batt_t = now;
    }
    b = plat_battery();
    if (b != ui.batt) {
        ui.batt = (int8_t)b;
        ui.redraw = 1;
    }
}

static void input(void)
{
    uint32_t keys, btn = plat_buttons(), k, now = plat_ms(), i;
    uint32_t lit_keys = 0, dim_keys = 0, mid_keys = 0, lit_btn, dim_btn;
    rhythm_t *r = cur();
    int32_t e;
    battery_poll(now);
    touch_poll();                        /* the sensed keys' presses and velocities */
    keys = touch_keys();
    if (ui.intro) {                      /* the opening: a key, a button or a knob ends it, and does nothing else */
        int any = (keys & ~ui.keys) || (btn & ~ui.btn);
        for (k = 0; k < NE; k++)
            any |= plat_enc((int)k) != 0;
        if (any) {
            ui.intro = 0;
            battery_show(5000u);
        }
        ui.keys = keys;
        ui.btn = btn;
        return;
    }
    if (drone()) {                       /* a drone played alone */
        solo_input(keys, btn);
        return;
    }
    for (i = 0; i < 16u; i++)            /* a white key down: a step */
        if ((keys >> WHITE[i] & 1u) && !(ui.keys >> WHITE[i] & 1u))
            step_key(i, WHITE[i]);
    for (i = 0; i < 11u; i++)            /* a black key down: a stroke */
        if ((keys >> BLACK[i] & 1u) && !(ui.keys >> BLACK[i] & 1u))
            pad_key(i, BLACK[i]);
    if (!berimbau() && !(keys >> BLACK[TRIANGLE_PAD] & 1u) && (ui.keys >> BLACK[TRIANGLE_PAD] & 1u))
        triangle_release();
    for (k = 0; k < NB; k++)
        if ((btn >> k & 1u) && !(ui.btn >> k & 1u))
            button(k);
    for (k = 0; k < NE; k++) {
        if ((e = plat_enc((int)k)) == 0)
            continue;
        if (save_list() && (k == EN_ALGO || k == EN_PRESET))   /* (the rhythm stays while it is being saved) */
            continue;
        if (k == EN_SELECT)
            inst_step(e);
        else if (k == EN_ALGO)
            meter_step(e > 0 ? 1 : -1);
        else if (k == EN_PRESET)
            rhythm_step(e > 0 ? 1 : -1);
        else if (k == EN_K3 && list_shown())   /* the presets' list: KNOB 3 goes through it */
            list_scroll(e);
        else
            knob_turn(page_knob(set.page, k - EN_K1), e);
    }
    hold_buttons(btn);
    {   /* the udu's bass key pressed: its bass lower, as far as the pressure goes (the palm closing the opening) */
        uint32_t uk = BLACK[UDU_BASS_PAD];
        float b = !berimbau() && (keys >> uk & 1u) ? -(float)set.bend * (float)touch_pressure(uk) * (1.0f / 127.0f)
                                                   : 0.0f;
        if (b != ui.bend) {
            ui.bend = b;
            dr_bend(I_UDU, 1, b);
        }
    }
    {   /* a berimbau's keys pressed: its gourd closes on the belly as far as the pressure goes (CABACA) */
        uint32_t gi = berimbau() && ber_idx(sinst()) >= 0 ? sinst() : I_BERIMBAU, p = 0;
        float c;
        for (i = 0; berimbau() && i < 3u; i++)
            if ((keys >> BLACK[i] & 1u) && touch_pressure(BLACK[i]) > p)
                p = touch_pressure(BLACK[i]);
        c = (float)set.cabaca * 0.01f * (float)p * (1.0f / 127.0f);
        if (gi != ui.gourd_inst || c != ui.gourd) {
            if (gi != ui.gourd_inst)
                dr_gourd(ui.gourd_inst, 0.0f);
            dr_gourd(gi, c);
            ui.gourd_inst = (uint8_t)gi;
            ui.gourd = c;
        }
    }
    if (keys != ui.keys)
        ui.redraw = 1;
    ui.keys = keys;
    ui.btn = btn;
    r = cur();
    if (seq.playing && !seq.count && seq.step >= 0 && npages(r) > 1u)   /* a long bar: the keys follow the step */
        ui.keypage = (uint8_t)((uint32_t)seq.step / page_steps(r));
    keypage_fit();
    /* the white keys: the instrument's strokes lit, the beats' first steps mid, the other steps dim; the step
     * sounding the other way round (a moving light) */
    for (i = 0; i < 16u; i++) {
        uint32_t s = ui.keypage * page_steps(r) + i, bit = 1u << WHITE[i];
        int on;
        if (i >= page_steps(r) || s >= nsteps(r))
            continue;
        on = ART(r->step[set.inst][s]) != 0u;
        if (seq.playing && !seq.count && seq.step == (int32_t)s)
            on = !on;
        if (on)
            lit_keys |= bit;
        else if (s % r->div == 0u)
            mid_keys |= bit;
        else
            dim_keys |= bit;
    }
    /* the black keys: all dim, the instrument's mid, lit while held or as their instrument sounds (none on it: dark) */
    for (i = 0; i < NINST; i++)
        if (seq.hits[i] != ui.hits_seen[i]) {
            ui.hits_seen[i] = seq.hits[i];
            ui.lit_until[i] = now + 90u;
        }
    for (i = 0; i < 11u; i++) {
        uint32_t bit = 1u << BLACK[i], art, row = pad_row(i, &art), in;
        if (row == PAD_VIRADA) {         /* VIRADA: lit while one waits for its bar */
            if ((keys & bit) || seq.vreq)
                lit_keys |= bit;
            else
                dim_keys |= bit;
            continue;
        }
        if (!art)
            continue;
        in = ROW_INST[berimbau()][row];
        if ((keys & bit) || (int32_t)(ui.lit_until[in] - now) > 0)
            lit_keys |= bit;
        else if (row == set.inst)
            mid_keys |= bit;
        else
            dim_keys |= bit;
    }
    if (seq.beats / bpm_per(cur()) != ui.beats_seen) {   /* SEQ blinks the beat */
        ui.beats_seen = seq.beats / bpm_per(cur());
        ui.beat_until = now + 80u;
    }
    lit_btn = (btn & ~(1u << B_PLAY)) | 1u << PAGE_BTN[set.page] | (set_dirty ? 1u << B_SAVE : 0u);
    if (seq.rec)                         /* PLAY: red recording, green playing (its two LEDs) */
        lit_btn |= 1u << B_PLAY;
    else if ((seq.playing && (!seq.stop_at || (now / 150u) & 1u)) || (btn >> B_PLAY & 1u))
        lit_btn |= LED_PLAY_GREEN;       /* (blinking: it stops on the bar's first step) */
    if (seq.rec && (!seq.count || (now / 150u) & 1u))   /* counting in: REC blinks */
        lit_btn |= 1u << B_REC;
    if (r->mute >> set.inst & 1u)
        lit_btn |= 1u << B_SEL;
    if (ui.clear_t && now - ui.clear_t < 2000u)
        lit_btn |= 1u << B_EDIT;
    if (ui.rise || (seq.playing && (int32_t)(ui.beat_until - now) > 0))
        lit_btn |= 1u << B_SEQ;
    if (list_shown() && !ui.list_save)
        lit_btn |= 1u << B_LFO;
    /* the ones that do something glow (ARP: the djembe's flams, the agogo) */
    dim_btn = ~lit_btn & ((1u << NB) - 1u) & ~(1u << B_PLAY);
    if (!(lit_btn & (1u << B_PLAY | LED_PLAY_GREEN)))
        dim_btn |= LED_PLAY_GREEN;       /* PLAY glows green */
    ui.glow = dim_btn;
    plat_lights(lit_btn, lit_keys, dim_btn, dim_keys, set.lights);
    plat_lights_mid(0, mid_keys);
}

void app_input_only(void) { input(); }

/* ----------------------------------------------------------------- screen --- */
/* The screen's rules: the greys for its structure (rules, empty steps, labels), white for values, an instrument's or a
 * drone's colour only for it (its name, its steps, its bars, the band), the accent (C_HI) only for what waits (a
 * message's word, the rhythm coming in at the next bar); green playing, red recording. No text under FONT_B. */
#define C_BOX RGB(20, 22, 26)
#define C_REC RGB(240, 60, 50)
#define C_GO RGB(90, 210, 110)

static uint16_t rgb_scale(uint16_t c, uint32_t q)   /* c * q / 256 */
{
    uint32_t r = (c >> 11) * q >> 8, g = (c >> 5 & 63u) * q >> 8, b = (c & 31u) * q >> 8;
    return (uint16_t)(r << 11 | g << 5 | b);
}

/* the screen in two bands of 120 rows, each drawn from the whole layout (rows outside the band are clipped) */
static void band(int32_t top)
{
    cv_begin(240, 120, C_BLACK);
    cv_oy = -top;
    cv_y0 = 0;
    cv_y1 = 120;
}

static void frame(int32_t x, int32_t y, int32_t w, int32_t h, int32_t t, uint16_t c)   /* a border t thick */
{
    cv_rect(x, y, w, t, c);
    cv_rect(x, y + h - t, w, t, c);
    cv_rect(x, y, t, h, c);
    cv_rect(x + w - t, y, t, h, c);
}

static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)   /* centred */
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}

static void solo_row(int32_t y, uint32_t i, const char *name, const char *value)   /* a setting: K, name, value */
{
    char k[3] = {'K', (char)('1' + i), 0};
    int32_t x = cv_text(8, y + 5, &FONT_B, k, C_DIM) + 6;
    cv_text(x, y, &FONT_M, name, C_GRAY);
    cv_text(232 - text_w(&FONT_M, value), y, &FONT_M, value, C_WHITE);
}

static void play_mark(int32_t x, int32_t y)   /* a green triangle, pointing right: sounding */
{
    int32_t i;
    for (i = 0; i < 13; i++)
        cv_rect(x + i, y + i / 2, 1, 16 - (i / 2) * 2, C_GO);
}

/* the battery, top right: its bars, three full, the outline red when it is empty; USB while a computer powers the
 * FM-1; nothing before its first reading */
static void battery_mark(void)
{
    int32_t i;
    uint16_t c = ui.batt ? C_GRAY : C_REC;
    if (ui.batt == 4)
        cv_text(234 - text_w(&FONT_B, "USB"), 9, &FONT_B, "USB", C_GRAY);
    if (ui.batt < 0 || ui.batt > 3)
        return;
    frame(211, 12, 21, 11, 1, c);
    cv_rect(232, 15, 2, 5, c);
    for (i = 0; i < ui.batt; i++)
        cv_rect(213 + i * 6, 14, 5, 7, C_GRAY);
}

/* a title, large; one too long to clear the battery, smaller */
static int32_t title(const char *t, uint16_t rgb)
{
    int big = text_w(&FONT_L, t) <= 196;
    return cv_text(8, big ? 2 : 6, big ? &FONT_L : &FONT_M, t, rgb);
}

/* the header: the rhythm large, in white (the one coming in at the next bar, in the accent); the page's title; on
 * SOUND the instrument in its colour (grey while muted); or a message */
static void header(void)
{
    const rhythm_t *r = cur();
    if (plat_ms() < ui.msg_until && ui.msg_title) {   /* a rhythm's name, as it will stay */
        int32_t x = title(ui.msg[0], ui.msg_rgb ? ui.msg_rgb : C_WHITE) + 10;
        if (ui.msg[1][0])
            cv_text(x, 11, &FONT_B, ui.msg[1], C_HI);
    } else if (plat_ms() < ui.msg_until) {
        int32_t x = cv_text(8, 6, &FONT_M, ui.msg[0], C_WHITE) + 10;
        const felucca_font_t *f = x + text_w(&FONT_M, ui.msg[1]) > 236 ? &FONT_B : &FONT_M;
        cv_text(x, f == &FONT_M ? 6 : 11, f, ui.msg[1], C_HI);
    } else if (save_list()) {
        cv_text(8, 2, &FONT_L, "SAVE TO", C_WHITE);
    } else if (set.page == PG_SETUP) {
        cv_text(8, 2, &FONT_L, "SETUP", C_WHITE);
    } else if (drone()) {
        cv_text(8, 2, &FONT_L, MODE_NAME[set.mode], MODE_RGB[set.mode]);
        if (drone_on())                 /* sounding: the green triangle, left of the battery */
            play_mark(190, 10);
    } else if (set.page == PG_HOME) {
        title(rhythm_name(rhythm_shown()), seq.next ? C_HI : C_WHITE);
    } else {
        int muted = (int)(r->mute >> set.inst & 1u);
        cv_text(8, 2, &FONT_L, shown()->name, muted ? C_DIM : shown()->rgb);
    }
    if (battery_shown() && (plat_ms() >= ui.msg_until || (ui.msg_title && !ui.msg[1][0])))
        battery_mark();                  /* (a message has the whole width, but for a title alone) */
    cv_rect(0, 36, 240, 1, C_LINE);
}

/* the steps: a row a beat, its steps across; a stroke fills its step in the instrument's colour (dimmer when soft)
 * with its letter; the step sounding has a thick white frame; the rows on the white keys a white mark at the left */
static void draw_steps(void)
{
    const rhythm_t *r = cur();
    const inst_t *in = shown();
    int32_t beats = r->beats, div = r->div, gap = 4, top = 42, h = 154;
    int32_t ch = (h - (beats - 1) * gap) / beats, cw = (226 - (div - 1) * gap) / div, b, c;
    uint32_t p0 = ui.keypage * page_steps(r), p1 = p0 + page_steps(r);
    int muted = (int)(r->mute >> set.inst & 1u);
    if (ch > 64)
        ch = 64;
    top += (h - (beats * ch + (beats - 1) * gap)) / 2;   /* a short bar sits in the middle */
    for (b = 0; b < beats; b++) {
        int32_t y = top + b * (ch + gap);
        uint32_t s0 = (uint32_t)(b * div);
        if (npages(r) > 1u && s0 >= p0 && s0 < p1)
            cv_rect(2, y, 3, ch, C_WHITE);
        for (c = 0; c < div; c++) {
            uint32_t s = s0 + (uint32_t)c, v = r->step[set.inst][s], a = ART(v);
            int32_t x = 10 + c * (cw + gap);
            if (a) {
                uint16_t col = muted ? C_GRAY : rgb_scale(in->rgb, 96u + VEL(v) * 160u / 127u);
                char t[2] = {in->letter[a - 1u], 0};
                const felucca_font_t *f = ch >= 28 ? &FONT_M : &FONT_B;
                int32_t ty = y + (ch - (ch >= 28 ? 24 : 16)) / 2;
                const char *w = in->word[a - 1u] && text_w(f, in->word[a - 1u]) <= cw - 6 ? in->word[a - 1u] : t;
                cv_rect(x, y, cw, ch, col);
                text_c(x + cw / 2, ty, f, w, C_BLACK);   /* its name where it fits (DOM, DIM, TCH), else its letter */
                if (FLAM(v))                 /* a flam: the grace stroke small before it */
                    cv_text(x + cw / 2 - text_w(f, t) / 2 - text_w(&FONT_XS, t) - 2, ty + (ch >= 28 ? 8 : 2),
                            &FONT_XS, t, C_BLACK);
            } else {
                cv_rect(x, y, cw, ch, c == 0 ? C_LINE : C_BOX);
            }
            if (seq.playing && !seq.count && seq.step == (int32_t)s)
                frame(x - 2, y - 2, cw + 4, ch + 4, 3, C_WHITE);
        }
    }
}

/* the footer: the tempo large, the time signature, the instrument shown, and what it is doing (a green
 * triangle playing, a red dot recording, the beats to go counting in, a white square stopping on the bar) */
static void footer(void)
{
    const rhythm_t *r = cur(), *n = &rhy[rhythm_shown()];   /* (the one coming in at the next bar, in the accent) */
    const char *name = shown()->name;
    char t[16];
    int32_t x;
    int_text((int32_t)bpm_shown(r), t);
    x = cv_text(8, 200, &FONT_M, t, C_WHITE);
    x = cv_text(x + 4, 205, &FONT_B, "BPM", ui.rise ? C_HI : C_GRAY);
    meter_text(n, t);
    cv_text(x + 8, 205, &FONT_B, t, seq.next ? C_HI : C_WHITE);
    cv_text(210 - text_w(&FONT_B, name), 205, &FONT_B, name, r->mute >> set.inst & 1u ? C_DIM : shown()->rgb);
    if (seq.count) {                     /* counting in: the beats to go */
        int_text((int32_t)((seq.count + r->div - 1u) / r->div), t);
        cv_text(232 - text_w(&FONT_M, t), 200, &FONT_M, t, C_REC);
    } else if (seq.rec) {
        cv_rect(218, 205, 14, 14, C_REC);
        cv_rect(218, 205, 2, 2, C_BLACK);    /* the corners off: a dot */
        cv_rect(230, 205, 2, 2, C_BLACK);
        cv_rect(218, 217, 2, 2, C_BLACK);
        cv_rect(230, 217, 2, 2, C_BLACK);
    } else if (seq.playing && seq.stop_at)   /* stopping on the bar's first step: a square */
        cv_rect(219, 205, 13, 13, C_WHITE);
    else if (seq.playing)
        play_mark(219, 204);
}

/* a page of four values, one a row: the name, the value large, a thick bar in the instrument's colour */
static void draw_page(void)
{
    uint32_t i;
    char t[24];
    uint16_t col = set.page == PG_SOUND ? shown()->rgb : C_HI;
    for (i = 0; i < 4u; i++) {
        uint32_t kn = page_knob(set.page, i);
        int32_t y = 42 + (int32_t)i * 48, f;
        if (kn == KN_NONE)
            continue;
        knob_text(kn, t);
        solo_row(y, i, KNOB_NAME[kn], t);
        if ((f = knob_fill(kn, 224)) >= 0) {
            cv_rect(8, y + 28, 224, 12, C_LINE);
            cv_rect(8, y + 28, f, 12, col);
            cv_rect(8 + (f > 3 ? f - 3 : 0), y + 25, 3, 18, C_WHITE);   /* where the value sits */
        }
    }
}

static void note_text(int32_t midi, char *t)   /* "C#3" */
{
    t[0] = 0;
    text_cat(t, NOTE_ABC[midi % 12]);
    int_text(midi / 12 - 1, t + text_len(t));
}

/* a drone's screen: the tanpura's four strings (the one just plucked filled), or the shruti's notes held (filled
 * while it sounds), large; its settings below */
static void draw_solo(void)
{
    uint16_t col = MODE_RGB[set.mode];
    char t[16];
    uint32_t i;
    if (set.mode == MODE_TANPURA) {
        static const int8_t OFF[4] = {0, 0, 0, -12};
        int plucked = drone_on() ? dn_string() : -1;
        for (i = 0; i < 4u; i++) {
            int32_t x = 8 + (int32_t)i * 57, midi = tanpura_sa() + (i ? OFF[i] : FIRST_OFF[set.tp_first]);
            int on = plucked == (int)i;
            if (on)
                cv_rect(x, 44, 52, 74, col);
            else {
                cv_rect(x, 44, 52, 74, C_BOX);
                frame(x, 44, 52, 74, 2, col);
            }
            text_c(x + 26, 52, &FONT_M, i ? "SA" : FIRST_NAME[set.tp_first], on ? C_BLACK : C_WHITE);
            note_text(midi, t);
            text_c(x + 26, 88, &FONT_B, t, on ? C_BLACK : C_GRAY);
        }
        int_text(set.tp_jawari, t);
        text_cat(t, "%");
        solo_row(126, 0, "JAWARI", t);
        int_text(set.tp_cycle / 10, t);
        text_cat(t, ".");
        int_text(set.tp_cycle % 10, t + text_len(t));
        text_cat(t, " S");
        solo_row(152, 1, "CYCLE", t);
        solo_row(178, 2, "REGISTER", set.tp_high ? "HIGH" : "LOW");
        int_text(set.tp_level, t);
        text_cat(t, "%");
        solo_row(204, 3, "LEVEL", t);
    } else {
        uint32_t n = 0, k;
        for (k = 0; k < NKEYS; k++) {
            int32_t x, y;
            if (!(set.sh_notes >> k & 1u))
                continue;
            if (n >= 8u) {               /* more than eight: how many more */
                n++;
                continue;
            }
            x = 8 + (int32_t)(n % 4u) * 57;
            y = 46 + (int32_t)(n / 4u) * 48;
            if (drone_on())
                cv_rect(x, y, 52, 42, col);
            else {
                cv_rect(x, y, 52, 42, C_BOX);
                frame(x, y, 52, 42, 2, col);
            }
            note_text(53 + (int32_t)k, t);
            text_c(x + 26, y + 9, &FONT_M, t, drone_on() ? C_BLACK : C_WHITE);
            n++;
        }
        {   /* the bellows: their breath as a bar from the left, lighter on the pull */
            int pull;
            float b = drone_on() ? dn_bellows(&pull) : 0.0f;
            int32_t w = (int32_t)(fm_clampf(b, 0.0f, 1.0f) * 224.0f + 0.5f);
            cv_rect(8, 142, 224, 6, C_LINE);
            if (w > 0)
                cv_rect(8, 142, w, 6, pull ? rgb_scale(col, 150u) : col);
        }
        if (!n)
            text_c(120, 82, &FONT_M, set.sh_kind == SH_NOTES ? "PRESS THE KEYS" : "PRESS A KEY", C_GRAY);
        else if (set.sh_kind != SH_NOTES) {   /* a chord: its name */
            shruti_name(t);
            text_c(120, 112, &FONT_M, t, col);
        } else if (n <= 4u)              /* the reeds one by one: said so, where there is room */
            text_c(120, 112, &FONT_M, "NOTES", C_GRAY);
        else if (n > 8u) {
            t[0] = '+';
            int_text((int32_t)(n - 8u), t + 1);
            text_c(120, 138, &FONT_B, t, C_GRAY);
        }
        t[0] = 0;
        if (set.sh_bellows) {
            int_text(set.sh_bellows, t);
            text_cat(t, "%");
        } else
            text_cat(t, "STEADY");
        solo_row(156, 0, "BREATHING", t);
        t[0] = 0;
        if (!set.sh_tempo)               /* the rhythm's tempo: said so */
            text_cat(t, "= ");
        int_text((int32_t)shruti_tempo(), t + text_len(t));
        text_cat(t, " BPM");
        solo_row(182, 1, "TEMPO", t);
        int_text(set.sh_level, t);
        text_cat(t, "%");
        solo_row(208, 2, "LEVEL", t);
    }
}

/* the presets' list (LFO): five at most, the one shown in the middle, large on the colour, with its beats; the one
 * chosen marked at the left */
static void draw_list(void)
{
    uint16_t col = drone() ? MODE_RGB[set.mode] : shown()->rgb;
    int32_t n = (int32_t)list_n(), lo = -(n - 1) / 2 < -2 ? -2 : -(n - 1) / 2, hi = n / 2 > 2 ? 2 : n / 2, o;
    char t[8];
    for (o = lo; o <= hi; o++) {
        uint32_t i = (uint32_t)(((int32_t)ui.list_i + o + n * 3) % n);
        int32_t y = 44 + (o + 2) * 38;
        uint16_t fg = o ? C_GRAY : C_BLACK;
        const felucca_font_t *f = o ? &FONT_B : &FONT_M;
        if (!o)
            cv_rect(6, y, 230, 34, col);
        if (i == list_now())
            cv_rect(0, y, 3, 34, C_WHITE);
        cv_text(14, y + (o ? 9 : 5), f, list_name(i), fg);
        list_value(i, t);
        if (t[0])
            cv_text(228 - text_w(f, t), y + (o ? 9 : 5), f, t, fg);
    }
}

/* the bottom row, under the footer: the mode's instruments in SELECT's order, always in their place, three letters
 * each; in their colour when they play in the rhythm (the one shown full, and taller), dark when not, grey when muted,
 * white as they strike; a drone sounding under it, last, in its colour */
static void band_insts(void)
{
    const rhythm_t *r = cur();
    uint32_t m = berimbau(), n = ROW_N[m], k = n + (ui.drone ? 1u : 0u), i;
    int32_t gap = k > 8u ? 1 : 2, w = (240 - ((int32_t)k - 1) * gap) / (int32_t)k;   /* (nine: the berimbau's, a drone) */
    for (i = 0; i < k; i++) {
        int32_t x = (int32_t)i * (w + gap), cw = i == k - 1u ? 240 - x : w, top;
        uint32_t row = i < n ? ROW_ORDER[m][i] : 0u;
        uint16_t rgb = i < n ? INST[ROW_INST[m][row]].rgb : MODE_RGB[ui.drone], c, fg = C_BLACK;
        const char *name = i < n ? INST[ROW_INST[m][row]].name : MODE_NAME[ui.drone];
        char t[4] = {name[0], name[1], name[2], 0};
        int here = i < n && row == set.inst;
        if (i == n)
            c = rgb;
        else if (r->mute >> row & 1u)
            c = C_DIM;
        else if (lad_out(row)) {         /* (the ladainha: not in yet) */
            c = C_LINE;
            fg = here ? rgb : C_DIM;
        } else if (seq.playing && !seq.count && seq.step >= 0 && ART(r->step[row][seq.step]))
            c = C_WHITE;
        else if (has_strokes(r, row))
            c = here ? rgb : rgb_scale(rgb, 192u);
        else {
            c = C_LINE;
            fg = here ? rgb : C_DIM;
        }
        top = here ? 222 : 226;
        cv_rect(x, top, cw, 240 - top, c);
        cv_text(x + (cw - text_w(&FONT_B, t)) / 2, 225, &FONT_B, t, fg);
    }
}

static void draw(void)
{
    int32_t top;
    for (top = 0; top < 240; top += 120) {
        band(top);
        header();
        if (list_shown())
            draw_list();
        else if (set.page == PG_SETUP)
            draw_page();
        else if (drone())
            draw_solo();
        else if (set.page == PG_HOME) {
            draw_steps();
            footer();
        } else
            draw_page();
        if (set.page == PG_HOME && !drone() && !list_shown())
            band_insts();
        else
            cv_rect(0, 232, 240, 8, set.page == PG_SETUP ? C_HI : drone() ? MODE_RGB[set.mode] : shown()->rgb);
        cv_blit(0, (uint32_t)top);
    }
}


/* ------------------------------------------------------------- the opening --- */
/* at start: the cerrado at dusk (art/opening-scene.webp) in big pixels, 80 x 80 of 32 colours, 3 x 3 on the screen
 * each, under the name; a maned wolf (the lobo-guara, art/opening.webp) walks into it from the left, slowing, and
 * stands, clear of the sun; then the scene a moment. A key, a button or a knob ends it at once. tools/opening.py
 * makes the scene and cuts his body out of his picture (opening.h); his legs are drawn here, each two bones from hip
 * or shoulder to foot. They walk: each foot on the ground while he goes over it (it does not slide), then lifted and
 * brought forward, the four in turn (near hind, near fore, far hind, far fore); a fore leg's knee bends forward, a
 * hind leg's back. At the end they settle as his picture has them */
#include "opening.h"
#define WALK_MS 2800u                    /* in from the left, */
#define INTRO_MS 4000u                   /* then the scene, still */
#define WALK_PX 60                       /* from out of sight, in the scene's pixels */
#define STRIDE 16.0f                     /* the ground a leg's step covers */
#define DUTY 0.62f                       /* of it, the foot on the ground */
#define LIFT 3.0f                        /* a foot lifted, at the most */
#define GROUND (66.0f + OP_DY)           /* where his feet stand */
#define SETTLE 0.7f                      /* from this much of the walk, his legs settle to the picture's */

static uint16_t op_pal[OP_FAR_FUR + 1u];   /* OP_PAL, as the canvas holds it */

static void op_dot(int32_t x, int32_t y, uint16_t c) { cv_rect(x * 3, y * 3, 3, 3, c); }   /* a big pixel */

/* a bone, two big pixels wide; the upper one three at its top, in fur down to fur (0 .. 1) of it, then the leg's */
static void op_bone(float x0, float y0, float x1, float y1, int upper, float fur, uint16_t c, uint16_t cf)
{
    int32_t n = (int32_t)(fm_maxf(fm_fabsf(x1 - x0), fm_fabsf(y1 - y0)) + 1.0f), i;
    for (i = 0; i <= n; i++) {
        float t = (float)i / (float)n;
        int32_t x = (int32_t)fm_floorf(x0 + (x1 - x0) * t + 0.5f), y = (int32_t)fm_floorf(y0 + (y1 - y0) * t + 0.5f);
        uint16_t k = t < fur ? cf : c;
        op_dot(x, y, k);
        op_dot(x + 1, y, k);
        if (upper && t < 0.5f)
            op_dot(x - 1, y, k);
    }
}

static float op_len(float x0, float y0, float x1, float y1) { return fm_sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0)); }

/* leg k (OP_LEG's order) with him dx along: its foot where the walk puts it, then (b, 0 .. 1) where the picture does */
static void op_leg(uint32_t k, float dx, float went, float b, uint16_t c, uint16_t cf)
{
    static const float OFF[4] = {0.5f, 0.75f, 0.0f, 0.25f};   /* each leg's turn in the step */
    const int8_t *g = OP_LEG[k];
    int fore = (int)(k & 1u);
    float hx = (float)g[0] + dx, hy = (float)(g[1] + OP_DY), p = went / STRIDE + OFF[k], rel, lift = 0.0f;
    float lw = fm_sqrtf((GROUND - hy) * (GROUND - hy) + 20.0f) * 0.5f;   /* a walking leg's bones */
    float l1 = lw + b * (op_len(g[0], g[1], g[2], g[3]) - lw), l2 = lw + b * (op_len(g[2], g[3], g[4], g[5]) - lw);   /* (the same lengths a little lower) */
    float fx, fy, vx, vy, d, a, h, sgn = fore ? -1.0f : 1.0f;
    p -= fm_floorf(p);
    if (p < DUTY)                        /* on the ground: it goes back under him as he goes on */
        rel = STRIDE * DUTY * (0.5f - p / DUTY);
    else {                               /* lifted, brought forward */
        float q = (p - DUTY) / (1.0f - DUTY);
        rel = STRIDE * DUTY * (q - 0.5f);
        lift = LIFT * fm_sinf(3.14159265f * q);
    }
    fx = hx + (fore ? 1.0f : -1.0f) + rel;
    fy = GROUND - lift;
    fx += b * ((float)g[4] + dx - fx);   /* settling: the picture's foot */
    fy += b * ((float)(g[5] + OP_DY) - fy);
    vx = fx - hx;
    vy = fy - hy;
    d = fm_sqrtf(vx * vx + vy * vy);
    if (d < 0.5f)
        return;
    if (d > l1 + l2 - 0.01f) {           /* out of reach: the leg straight towards it */
        float s = (l1 + l2 - 0.01f) / d;
        vx *= s;
        vy *= s;
        d = l1 + l2 - 0.01f;
    }
    a = (l1 * l1 - l2 * l2 + d * d) / (2.0f * d);
    h = fm_sqrtf(fm_maxf(0.0f, l1 * l1 - a * a));
    {
        float kx = hx + vx * a / d - sgn * vy * h / d, ky = hy + vy * a / d + sgn * vx * h / d;
        op_bone(hx, hy, kx, ky, 1, 0.45f, c, cf);   /* rust fur at the top, then black */
        op_bone(kx, ky, hx + vx, hy + vy, 0, 0.0f, c, c);
        op_dot((int32_t)fm_floorf(hx + vx + 0.5f) + 2, (int32_t)fm_floorf(hy + vy + 0.5f), c);   /* the paw */
    }
}

static void draw_intro(uint32_t t)
{
    float u = t < WALK_MS ? (float)t / (float)WALK_MS : 1.0f, left = (1.0f - u) * (1.0f - u);   /* slowing */
    float b = u > SETTLE ? (u - SETTLE) / (1.0f - SETTLE) : 0.0f;
    int32_t bx = OP_STOP - (int32_t)((float)WALK_PX * left + 0.5f), top, x, y;
    uint32_t k;
    b = b * b * (3.0f - 2.0f * b);
    for (top = 0; top < 240; top += 120) {
        band(top);
        for (y = 0; y < 120; y++) {      /* the scene, 3 x 3 a pixel */
            const uint8_t *src = OP_SCENE + (uint32_t)((top + y) / 3) * OP_N;
            uint16_t *row = cv_px + (uint32_t)y * 240u;
            for (x = 0; x < 240; x += 3)
                row[x] = row[x + 1] = row[x + 2] = op_pal[src[x / 3]];
        }
        text_c(120, 9, &FONT_L, APP_NAME, RGB(250, 232, 196));   /* the name, in the screen's letters */
        for (k = 0; k < 4u; k++)         /* his legs, the far ones darker; then his body over their tops */
            op_leg(k, (float)bx, (float)(WALK_PX + bx - OP_STOP), b, OP_PAL[k < 2u ? OP_FAR : OP_NEAR],
                   OP_PAL[k < 2u ? OP_FAR_FUR : OP_NEAR_FUR]);
        for (y = 0; y < OP_BODY_H; y++)
            for (x = 0; x < OP_BODY_W; x++) {
                uint32_t i = OP_BODY[y * OP_BODY_W + x];
                if (i != OP_CLEAR)
                    op_dot(OP_BODY_X + x + bx, OP_BODY_Y + y + OP_DY, OP_PAL[i]);
            }
        cv_blit(0, (uint32_t)top);
    }
}

static void intro_frame(uint32_t now)
{
    uint32_t t = now - ui.intro_t0, step = (t < WALK_MS ? t : WALK_MS) / 40u;
    if (t >= INTRO_MS) {                 /* the rhythm box */
        ui.intro = 0;
        battery_show(5000u);
    } else if ((int32_t)step != ui.intro_drawn) {   /* every 40 ms while he walks; once, standing */
        ui.intro_drawn = (int32_t)step;
        draw_intro(t);
    }
}

/* ------------------------------------------------------------------- start --- */
static void settings_default(void)
{
    uint32_t i;
    set.magic = SET_MAGIC;
    set.rhythm = 0;
    set.inst = 0;
    set.page = PG_HOME;
    set.click = CLICK_REC;
    set.countin = 1;
    set.lights = 2;
    set.fixed = 0;
    set.bend = 3;
    for (i = 0; i < NINST; i++)
        *snd_of(i) = INST[i].def;
    set.mode = MODE_KIT;
    set.tp_sa = 1;                       /* C#: MALE KALI 1 */
    set.tp_first = 0;                    /* Pa */
    set.tp_high = 0;
    set.tp_cycle = 40;
    set.tp_level = 70;
    set.sh_level = 70;
    set.sh_bellows = 40;
    set.sh_notes = 1u << 7 | 1u << 14 | 1u << 19;   /* Sa Pa Sa' on C: C4, G4, C5 */
    set.other_rhythm = NRHY_KIT;         /* the berimbau: ANGOLA */
    set.other_inst = I_BERIMBAU;
    set.tp_preset = 2;                   /* RAGA */
    set.tp_jawari = 75;
    set.sh_kind = SH_NOTES;              /* the reeds one by one (SA PA would play the same on C4) */
    set.sh_root = 7;
    set.sh_tempo = 40;                   /* a push and a pull in 3 s */
    for (i = 0; i < NRHY; i++)
        set.bpm[i] = 0;                  /* as written */
    set.cabaca = 80;
    set.virada[0] = set.virada[1] = 0;   /* the gunga and the medio keep to the toque; */
    set.virada[2] = 2;                   /* the viola varies it every 4 bars (a sourced variation: 2 of them) */
    set.maracas = 0;                     /* the shaker */
}

void app_init(void)
{
    uint32_t i;
    int ok;
    settings_default();                  /* (an older record, shorter, leaves what follows it as at first) */
    ok = plat_store_load(OBJ_SETTINGS, &set, sizeof set) >= (int)SET_V09 && set.magic == SET_MAGIC
             && set.rhythm < NRHY && set.other_rhythm < NRHY && rhy_mode(set.rhythm) != rhy_mode(set.other_rhythm)
             && set.page < NPAGES && set.click <= CLICK_ON
             && set.countin <= 1u && set.lights >= 1u && set.lights <= 2u && set.fixed <= 1u && set.bend <= 6u
             && set.mode < NMODES && set.tp_sa < 12u && set.tp_first < 3u && set.tp_high <= 1u && set.tp_cycle >= 20u
             && set.tp_cycle <= 80u && set.tp_level <= 100u && set.sh_level <= 100u && set.sh_bellows <= 100u
             && set.sh_notes < 1u << NKEYS && set.tp_preset < NTP && set.tp_jawari <= 100u && set.sh_kind < NSH && set.sh_root < NKEYS
             && (!set.sh_tempo || (set.sh_tempo >= 20u && set.sh_tempo <= 120u));
    for (i = 0; ok && i < NINST; i++) {
        const dr_inst_t *d = snd_of(i);
        ok = d->level >= 0.0f && d->level <= 1.0f && d->tune >= -TUNE_MAX && d->tune <= TUNE_MAX && d->decay >= 0.1f
             && d->decay <= 1.0f && d->wires >= 0.0f && d->wires <= 1.0f;
    }
    ok = ok && set.cabaca <= 100u && set.virada[0] <= 4u && set.virada[1] <= 4u && set.virada[2] <= 4u
         && set.maracas < NMARACAS;
    for (i = 0; ok && i < NRHY; i++)
        ok = !set.bpm[i] || (set.bpm[i] >= 40u && set.bpm[i] <= 240u);
    if (!ok)
        settings_default();
    if (!inst_in(set.inst, rhy_mode(set.rhythm)))   /* (1.0's kit had a seventh, the agogo) */
        set.inst = 0;
    if (!inst_in(set.other_inst, rhy_mode(set.other_rhythm)))
        set.other_inst = 0;
    rhythm_box_fit();
    for (i = 0; i < NRHY; i++)           /* the rhythms as written; then the ones kept, each that is sound */
        rhythm_default(i);
    if (plat_store_load(OBJ_RHYTHMS, &keep, sizeof keep) == (int)sizeof keep && keep.magic == RHY_MAGIC)
        for (i = 0; i < NKEEP; i++) {
            const rhythm_t *r = &keep.r[i];
            uint32_t k = keep.which[i];
            if (k < NRHY && (RDEF[k].free ? r->beats >= 2u && r->beats <= 8u : r->beats == RDEF[k].beats)
                && r->div >= 2u && r->div <= 4u && r->bpm >= 40u && r->bpm <= 240u && r->swing <= 100u
                && r->human <= 100u)
                rhy[k] = *r;
        }
    for (i = 0; i < NRHY; i++) {         /* (1.0's agogo row in the kit's rhythms: not played any more) */
        uint32_t row, s;
        for (row = 0; row < NROW; row++)
            for (s = 0; row_inst(i, row) == I_NONE && s < NSTEPS; s++)
                rhy[i].step[row][s] = 0;
    }
    keep_init();                         /* (0.9's and 1.0's records end with the two chosen: not kept any more) */
    ord_init();
    seq.rnd = 0x2545F491u;
    dr_init(SR);
    for (i = 0; i < NINST; i++)
        sound_apply(i);
    dn_init(SR);
    drone_apply();                       /* (silent: a drone starts with PLAY) */
}

void app_ui_init(void)
{
    uint32_t i;
    touch_on(1);                         /* velocity: the keys' readings on */
    ui.list_on = 0;
    ui.batt = -1;
    ui.tri_step = -1;
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.drawn_step = -2;
    ui.redraw = 1;
    for (i = 0; i <= OP_FAR_FUR; i++)    /* the opening first */
        op_pal[i] = swap16(OP_PAL[i]);
    ui.intro = 1;
    ui.intro_t0 = plat_ms();
    ui.intro_drawn = -1;
}

void app_frame(void)
{
    uint32_t now = plat_ms();
    static uint32_t last_count;
    input();
    speak();
    if (ui.intro) {
        intro_frame(now);
        return;
    }
    rise_poll();
    if (ui.msg_until && now >= ui.msg_until) {
        ui.msg_until = 0;
        ui.redraw = 1;
    }
    if (ui.batt_until && now >= ui.batt_until) {
        ui.batt_until = 0;
        ui.redraw = 1;
    }
    if (set.mode == MODE_SHRUTI && drone_on() && !list_shown() && set.page == PG_HOME
        && now - ui.bellows_t >= 80u) {  /* the bellows' bar moving */
        ui.bellows_t = now;
        ui.redraw = 1;
    }
    if (set.mode == MODE_TANPURA && drone_on() && dn_string() != ui.string_drawn) {   /* the next string plucked */
        ui.string_drawn = (int8_t)dn_string();
        ui.redraw = 1;
    }
    if (ui.switched != seq.switched) {   /* the rhythm chosen came in on its bar */
        ui.switched = seq.switched;
        rhythm_shown_fit();
        drone_apply();                   /* (its tempo: the shruti's bellows may follow it) */
    }
    if (ui.drawn_step != (seq.playing ? seq.step : -2) || last_count != seq.count) {   /* the step moved on */
        ui.drawn_step = seq.playing ? seq.step : -2;
        last_count = seq.count;
        if (set.page == PG_HOME)
            ui.redraw = 1;
    }
    if (ui.redraw) {
        ui.redraw = 0;
        draw();
    }
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    const rhythm_t *r = cur();
    if (!strcmp(what, "inst"))
        *got = set.inst;
    else if (!strcmp(what, "rhythm"))
        *got = set.rhythm;
    else if (!strcmp(what, "beats"))
        *got = r->beats;
    else if (!strcmp(what, "div"))
        *got = r->div;
    else if (!strcmp(what, "bpm"))
        *got = r->bpm;
    else if (!strcmp(what, "bpmshown"))  /* the tempo on the screen */
        *got = (int)bpm_shown(r);
    else if (!strcmp(what, "swing"))
        *got = r->swing;
    else if (!strcmp(what, "human"))
        *got = r->human;
    else if (!strcmp(what, "playing"))
        *got = seq.playing;
    else if (!strcmp(what, "meter"))     /* the time signature shown: 404 is 4/4, 1208 is 12/8 */
        *got = (int)(meter_top(&rhy[rhythm_shown()]) * 100u + meter_low(&rhy[rhythm_shown()]));
    else if (!strcmp(what, "rec"))
        *got = seq.rec;
    else if (!strcmp(what, "count"))
        *got = (int)seq.count;
    else if (!strcmp(what, "step"))
        *got = seq.step;
    else if (!strcmp(what, "vreq"))      /* VIRADA waits for its bar */
        *got = seq.vreq != 0u;
    else if (!strcmp(what, "vary"))      /* the rows varying this bar */
        *got = seq.vary;
    else if (!strcmp(what, "vph"))       /* the gunga's sourced phrase: 1 + its set << 3 | its place; 0 none */
        *got = seq.vph[0];
    else if (!strcmp(what, "gourd"))     /* the berimbau shown's gourd, % closed */
        *got = (int)(ui.gourd * 100.0f + 0.5f);
    else if (!strcmp(what, "next"))      /* a rhythm chosen waits for its bar */
        *got = seq.next != 0u;
    else if (!strcmp(what, "stopat"))    /* PLAY waits for the bar's first step to stop */
        *got = seq.stop_at;
    else if (!strcmp(what, "bpmnow"))    /* the tempo playing (gliding), whole BPM */
        *got = (int)(seq.bpm_now + 0.5f);
    else if (!strcmp(what, "page"))
        *got = set.page;
    else if (!strcmp(what, "batt"))      /* the battery shown: 0..3 bars, 4 USB power, -1 none yet */
        *got = ui.batt;
    else if (!strcmp(what, "keypage"))
        *got = ui.keypage;
    else if (!strcmp(what, "muted"))
        *got = (int)(r->mute >> set.inst & 1u);
    else if (!strcmp(what, "dirty"))
        *got = set_dirty;
    else if (!strcmp(what, "voices"))
        *got = (int)dr_active();
    else if (!strcmp(what, "click"))
        *got = set.click;
    else if (!strcmp(what, "countin"))
        *got = set.countin;
    else if (what[0] == 's' && what[1] >= '0' && what[1] <= '9')   /* sN: the instrument's stroke at step N */
        *got = (int)ART(r->step[set.inst][atoi(what + 1) % NSTEPS]);
    else if (!strncmp(what, "flam", 4))  /* flamN: the instrument's step N is a flam */
        *got = (int)FLAM(r->step[set.inst][atoi(what + 4) % NSTEPS]);
    else if (!strcmp(what, "bend"))      /* the udu's bass: its glide now, tenths of a semitone */
        *got = (int)(dr_bent(I_UDU) * 10.0f + (dr_bent(I_UDU) < 0.0f ? -0.5f : 0.5f));
    else if (!strcmp(what, "wires"))     /* the instrument's snare wires, % */
        *got = (int)(snd_of(sinst())->wires * 100.0f + 0.5f);
    else if (!strcmp(what, "tune"))      /* the instrument's tuning, tenths of a semitone */
        *got = (int)(snd_of(sinst())->tune * 10.0f + (snd_of(sinst())->tune < 0.0f ? -0.5f : 0.5f));
    else if (what[0] == 'v' && what[1] >= '0' && what[1] <= '9')   /* vN: its velocity */
        *got = ART(r->step[set.inst][atoi(what + 1) % NSTEPS]) ? (int)VEL(r->step[set.inst][atoi(what + 1) % NSTEPS]) : 0;
    else if (!strcmp(what, "maracas"))   /* the maracas' type, 0 the shaker .. */
        *got = set.maracas;
    else if (!strcmp(what, "marasound"))   /* the stroke of the maracas sounding: 1 + its type, 0 none */
        *got = (int)dr_art_on(I_MARACAS);
    else if (!strcmp(what, "maratake"))   /* the take their next stroke plays */
        *got = (int)dr_take_next(I_MARACAS, set.maracas + 1u);
    else if (!strncmp(what, "hits", 4))  /* hitsI: instrument I's strokes played since boot */
        *got = (int)seq.hits[atoi(what + 4) % NINST];
    else if (!strncmp(what, "glow", 4)) {   /* glowFX, glowLFO, glowARP, glowOCT: it glows (OCT: OCT- or OCT+) */
        uint32_t m = !strcmp(what + 4, "FX") ? 1u << B_FX : !strcmp(what + 4, "LFO") ? 1u << B_LFO
                     : !strcmp(what + 4, "ARP") ? 1u << B_ARP : 1u << B_OCTDN | 1u << B_OCTUP;
        *got = (ui.glow & m) != 0u;
    } else if (!strcmp(what, "mode"))    /* the kit 0, the berimbau 1, the tanpura 2, the shruti 3 */
        *got = set.mode;
    else if (!strcmp(what, "drone"))     /* the drone shown sounds */
        *got = drone_on();
    else if (!strcmp(what, "dronebg"))   /* the drone sounding, shown or not: 2 the tanpura, 3 the shruti, 0 none */
        *got = ui.drone;
    else if (!strcmp(what, "sa"))        /* the tanpura's Sa, 0 C .. 11 B */
        *got = set.tp_sa;
    else if (!strcmp(what, "first"))     /* its first string: 0 Pa, 1 Ma, 2 Ni */
        *got = set.tp_first;
    else if (!strcmp(what, "string"))    /* its string plucked last */
        *got = dn_string();
    else if (!strcmp(what, "notes")) {   /* the shruti's notes held */
        uint32_t m = set.sh_notes;
        *got = 0;
        for (; m; m &= m - 1u)
            (*got)++;
    } else if (!strcmp(what, "msg"))       /* a message is on the screen */
        *got = ui.msg_until && (int32_t)(ui.msg_until - plat_ms()) > 0;
    else if (!strcmp(what, "intro"))     /* the opening is playing */
        *got = ui.intro;
    else if (!strcmp(what, "list"))      /* the presets' list is on the screen */
        *got = list_shown();
    else if (!strcmp(what, "listi"))     /* the one it shows */
        *got = ui.list_i;
    else if (!strcmp(what, "rise"))      /* the tempo rising */
        *got = ui.rise;
    else if (!strcmp(what, "lad"))       /* the ladainha: 1 its base, 2 the chula */
        *got = seq.lad;
    else if (!strcmp(what, "ladrows"))   /* the rows that have come in */
        *got = seq.lad_rows;
    else if (!strcmp(what, "battshown")) /* the battery on the screen */
        *got = battery_shown();
    else if (!strcmp(what, "listn"))     /* how many it has */
        *got = (int)list_n();
    else if (!strcmp(what, "savelist"))  /* it is SAVE's */
        *got = save_list();
    else if (!strcmp(what, "tpp"))       /* the tanpura's character (PRESETS) */
        *got = set.tp_preset;
    else if (!strcmp(what, "jawari"))    /* its jawari, % */
        *got = set.tp_jawari;
    else if (!strcmp(what, "cycle"))     /* its cycle, tenths of a second */
        *got = set.tp_cycle;
    else if (!strcmp(what, "kind"))      /* the shruti's: 0 notes, 1 SA PA, 2 major, 3 minor */
        *got = set.sh_kind;
    else if (!strcmp(what, "root"))      /* its chord's key */
        *got = set.sh_root;
    else if (!strcmp(what, "shtempo"))   /* its bellows' strokes a minute */
        *got = set.sh_tempo;
    else if (!strcmp(what, "strokes")) { /* the instrument's steps with a stroke, in the bar */
        uint32_t s;
        *got = 0;
        for (s = 0; s < nsteps(r); s++)
            *got += ART(r->step[set.inst][s]) != 0u;
    } else
        return 0;
    return 1;
}
#endif
