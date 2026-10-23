/* SPDX-License-Identifier: MPL-2.0 */
/* NEW: a rhythm box of ceremony percussion. Six instruments (djembe, cajon, udu, clapsticks, maracas, triangle), each
 * with its strokes; a rhythm is a bar of 2 to 8 beats, 2 to 4 steps a beat, and each instrument's strokes on its steps.
 * The white keys are the selected instrument's steps (a press: its next stroke, then none); the black keys play the
 * instruments, grouped as they are: djembe 3, cajon 2, udu 3, clapsticks and maracas, triangle. Each setting has one
 * control: SELECT the instrument, ALGORITHM the beats (2 to 8), PRESET the rhythm among those with that many beats
 * (each as it is written; each number of beats has a FREE one, the player's own), PLAY plays it in a loop (PLAY again stops),
 * REC records what the black keys play onto the nearest step (from a stop, after a bar counted in), SEL mutes the
 * instrument, EDIT twice clears it, SEQ taps the tempo, ARP held makes the djembe's strokes flams (a step's, a black
 * key's), OCT- / OCT+ the keys' page of steps when a bar has more than the 16 white keys, SAVE keeps everything, MASTER
 * the volume. On the home page KNOB 1-4 set the tempo, the steps a beat, the swing (with 2 or 4 steps a beat) and the
 * human feel; ENV opens the instrument's SOUND, with only what a player of it can change (its level; the djembe's and
 * the udu's tuning; the djembe's, the maracas' and the triangle's decay; how far pressing the udu's bass key lowers it,
 * as a palm closing the pot's opening), GLO the SETUP.
 * The sequencer runs in the audio interrupt, to the sample. Part of the unity build (device/unity.c,
 * host/plat_host.c), after the platform: gfx.c and announce.c are in scope. */
#include "fastmath.h"
#include "drums.h"

/* ------------------------------------------------------------ the instruments --- */
#define NINST DR_NINST
typedef struct {
    const char *name;
    uint16_t rgb;
    uint8_t nart;
    const char *art[DR_NART];            /* the strokes' names */
    char letter[DR_NART];                /* on the steps, and in the rhythms below */
    dr_inst_t def;                       /* the sound at first: level, tune, decay */
} inst_t;
static const inst_t INST[NINST] = {
    {"DJEMBE", RGB(240, 140, 40), 3, {"BASS", "TONE", "SLAP"}, {'B', 'T', 'S'}, {0.85f, 0.0f, 1.0f}},
    {"CAJON", RGB(236, 206, 72), 2, {"BASS", "SLAP"}, {'B', 'S', 0}, {0.9f, 0.0f, 1.0f}},
    {"UDU", RGB(222, 82, 60), 3, {"BASS", "TONE", "SLAP"}, {'B', 'T', 'S'}, {0.85f, 0.0f, 1.0f}},
    {"CLAPSTICKS", RGB(124, 204, 84), 1, {"HIT", 0, 0}, {'X', 0, 0}, {0.65f, 0.0f, 1.0f}},
    {"MARACAS", RGB(232, 96, 184), 1, {"SHAKE", 0, 0}, {'X', 0, 0}, {0.7f, 0.0f, 1.0f}},
    {"TRIANGLE", RGB(92, 192, 242), 2, {"OPEN", "MUTED", 0}, {'O', 'M', 0}, {0.5f, 0.0f, 1.0f}},
};
/* the black keys, left to right: instrument and stroke */
static const uint8_t PAD[11][2] = {{0, 1}, {0, 2}, {0, 3}, {1, 1}, {1, 2}, {2, 1}, {2, 2}, {2, 3}, {3, 1}, {4, 1},
                                   {5, 1}};
static const uint8_t WHITE[16] = PLAT_WHITE_KEYS, BLACK[11] = PLAT_BLACK_KEYS;
enum { I_DJEMBE, I_CAJON, I_UDU };
#define UDU_BASS_PAD 5u                  /* the black key of the udu's bass: its pressure lowers it */
#define FLAM_S 0.028f                    /* a flam: a soft grace stroke, the stroke this long after */
#define FLAM_GRACE 0.45f

/* ---------------------------------------------------------------- the rhythms --- */
#define NSTEPS 32u                       /* 8 beats of 4 */
#define NRHY 23u                         /* kept in two store objects */
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
    uint8_t mute;                        /* bit i: instrument i silent */
    uint8_t rsv[2];
    uint8_t step[NINST][NSTEPS];
} rhythm_t;
#define RHY_MAGIC 0x34594852u            /* "RHY4" */
static struct {
    uint32_t magic;
    rhythm_t r[NRHY];
} rhy;

/* the rhythms at first: one string an instrument, a character a step: the stroke's letter (capital: strong, small:
 * soft), "." none. From the literature:
 *   MARCHA, VALSA, MAZURCA: Santo Daime's three rhythms, as its maracá plays them (f soft, F strong): marcha f f f F,
 *     valsa F F f, mazurca (6/8) f f f F F F (Jornal Grande Bahia, "Abaixo o maracá. Viva o maracá", 2016)
 *   IJEXA, BARRAVENTO: Umbanda's atabaque toques on the djembe, Tum its bass, Ta its tone, the marking stroke (M)
 *     strong: Ijexá "Ta TaTa Tum Ta", Barravento (6/8) "Tum TaTa TumTum" (Templo Tata Possum, Curso de Atabaque, 2024)
 *   BAIAO: the zabumba on the first beat's first and fourth sixteenths (here the djembe's bass), the triangle's
 *     sixteenths closed, open on the off-beats (ANPPOM 2018); SAMBA: the surdo soft on 1, strong on 2 (the cajon's
 *     bass), the ganzá's sixteenths (the maracas) */
typedef struct {
    const char *name;
    uint8_t beats, div, bpm, swing;
    uint8_t free;                        /* the player's own: kept as left, not written again when chosen */
    const char *pat[NINST];              /* djembe, cajon, udu, clapsticks, maracas, triangle */
} rdef_t;
/* grouped by their beats (ALGORITHM), each group ending with its FREE one (PRESET goes round a group) */
static const rdef_t RDEF[] = {
    {"HEARTBEAT", 2, 4, 64, 0, 0, {"Bb..Bb..", "", "B.......", "", "..x...x.", ""}},
    {"BAIAO", 2, 4, 100, 0, 0, {"B..B....", "", "", "", "", "MMOMMMOM"}},
    {"SAMBA", 2, 4, 100, 0, 0, {"", "b...B...", "", "", "xxxXxxxX", ""}},
    {"MAZURCA", 2, 3, 60, 0, 0, {"", "", "", "", "xxxXXX", ""}},
    {"BARRAVENTO", 2, 3, 72, 0, 0, {"B.ttbb", "", "", "", "", ""}},
    {"FREE", 2, 4, 90, 0, 1, {"", "", "", "", "", ""}},
    {"THREE", 3, 4, 84, 0, 0, {"", "B...S...S...", "", "", "X.x.X.x.X.x.", "O..........."}},
    {"VALSA", 3, 2, 120, 0, 0, {"", "", "", "", "X.X.x.", ""}},
    {"FREE", 3, 4, 90, 0, 1, {"", "", "", "", "", ""}},
    {"FOUR", 4, 4, 92, 0, 0, {"", "B...S...B.b.S...", "", "", "x.X.x.X.x.X.x.X.", "O.......M......."}},
    {"JOURNEY", 4, 2, 116, 0, 0, {"BbbbBbbb", "", "B.......", "", "", ""}},
    {"SHAKER", 4, 4, 76, 0, 0, {"", "", "B.......b.......", "", "XxxxXxxxXxxxXxxx", "O..............."}},
    {"CLAVE", 4, 4, 100, 0, 0, {"....T.T.....T.TS", "B.......B.......", "", "X..X..X...X.X...", "x.x.x.x.x.x.x.x.", ""}},
    {"AFRO 12/8", 4, 3, 100, 0, 0, {"S..TT.S..TT.", "", "B.....b.....", "X.X.XX.X.X.X", "X.xX.xX.xX.x", ""}},
    {"MARCHA", 4, 2, 120, 0, 0, {"", "", "", "", "x.x.x.X.", ""}},
    {"IJEXA", 4, 2, 96, 0, 0, {"T.ttB.t.", "", "", "", "x.x.x.x.", ""}},
    {"FREE", 4, 4, 90, 0, 1, {"", "", "", "", "", ""}},
    {"FIVE", 5, 2, 110, 0, 0, {"", "B.s.s.B.S.", "", "", "XxXxXxXxXx", "O........."}},
    {"FREE", 5, 2, 90, 0, 1, {"", "", "", "", "", ""}},
    {"FREE", 6, 2, 90, 0, 1, {"", "", "", "", "", ""}},
    {"SEVEN", 7, 2, 150, 0, 0, {"", "B...S...B...S.", "", "X...X...X.....", "x.x.x.x.x.x.x.", ""}},
    {"FREE", 7, 2, 90, 0, 1, {"", "", "", "", "", ""}},
    {"FREE", 8, 2, 90, 0, 1, {"", "", "", "", "", ""}},
};
typedef char rdef_one_each[sizeof RDEF / sizeof RDEF[0] == NRHY ? 1 : -1];   /* a rhythm for each place */

/* the rhythms in two store objects: the first half with the magic, then the second */
#define RHY_A (sizeof rhy.magic + NRHY / 2u * sizeof(rhythm_t))
#define RHY_B ((NRHY - NRHY / 2u) * sizeof(rhythm_t))

static void rhythm_default(uint32_t k)
{
    rhythm_t *r = &rhy.r[k];
    const rdef_t *d = &RDEF[k];
    uint32_t i, s, a;
    for (i = 0; i < sizeof *r; i++)
        ((uint8_t *)r)[i] = 0;
    r->beats = d->beats;
    r->div = d->div;
    r->bpm = d->bpm;
    r->swing = d->swing;
    for (i = 0; i < NINST; i++)
        for (s = 0; s < NSTEPS && d->pat[i][s]; s++)
            for (a = 0; a < INST[i].nart; a++) {
                char c = d->pat[i][s];
                if (c == INST[i].letter[a])
                    r->step[i][s] = STEP(a + 1u, 112u, 0u);
                else if (c == INST[i].letter[a] + 32)   /* small: soft */
                    r->step[i][s] = STEP(a + 1u, 70u, 0u);
            }
}

/* -------------------------------------------------------------- the settings --- */
enum { PG_HOME, PG_SOUND, PG_SETUP, NPAGES };
static const char *const PAGE_NAME[NPAGES] = {"HOME", "SOUND", "SETUP"};
static const uint8_t PAGE_BTN[NPAGES] = {B_HOME, B_ENV, B_GLO};
enum { CLICK_OFF, CLICK_REC, CLICK_ON };
#define SET_MAGIC 0x32534452u            /* "RDS2": the settings record; another magic loads defaults */
typedef struct {
    uint32_t magic;
    uint8_t rhythm, inst, page;
    uint8_t click;                       /* the metronome: off, while recording (and counting in), always */
    uint8_t countin;                     /* REC from a stop counts a bar in first */
    uint8_t lights;                      /* the empty steps' lights: 1 low, 2 high */
    uint8_t fixed;                       /* the keys' velocity ignored: every stroke at 100 */
    uint8_t bend;                        /* the udu's bass key pressed all the way: this many semitones lower, 0 .. 6 */
    dr_inst_t snd[NINST];
} settings_t;
static settings_t set;
static uint8_t set_dirty;                /* changed since the last save: SAVE is lit */

static rhythm_t *cur(void) { return &rhy.r[set.rhythm]; }
static uint32_t nsteps(const rhythm_t *r) { return (uint32_t)r->beats * r->div; }
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
    volatile uint8_t skip[NINST];        /* 1 + a step: that instrument's next stroke there is already played */
    volatile uint32_t hits[NINST];       /* strokes played, each instrument (the pads' lights, the checks) */
    volatile uint32_t beats;             /* beats played (SEQ's light) */
    uint32_t rnd;
} seq;
static struct {                          /* the black keys' strokes, from the main loop to the audio interrupt */
    uint8_t inst[16], art[16], vel[16];
    uint16_t delay[16];
    volatile uint32_t w, r;
} padq;

static float rnd01(void)
{
    seq.rnd = seq.rnd * 1664525u + 1013904223u;
    return (float)(seq.rnd >> 8) * (1.0f / 16777216.0f);
}

static float step_len(const rhythm_t *r, uint32_t s)   /* samples; with swing, the off-steps late */
{
    float l = SR * 60.0f / ((float)r->bpm * (float)r->div);
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
        seq.len = step_len(r, s);
        return;
    }
    s = seq.step < 0 || (uint32_t)seq.step + 1u >= ns ? 0u : (uint32_t)seq.step + 1u;
    seq.step = (int32_t)s;
    seq.len = step_len(r, s);
    if (s % r->div == 0u) {
        seq.beats++;
        if (click_now())
            dr_click(s == 0u, at);
    }
    for (i = 0; i < NINST; i++) {
        uint32_t v = r->step[i][s], d = at;
        float vel;
        if (!ART(v))
            continue;
        if (seq.skip[i] == s + 1u) {     /* recorded ahead of the step: it was played as it was played */
            seq.skip[i] = 0;
            continue;
        }
        if (r->mute >> i & 1u)
            continue;
        vel = (float)VEL(v) * (1.0f / 127.0f);
        if (r->human) {                  /* up to 12 ms late and 30 % softer */
            float h = (float)r->human * 0.01f;
            d += (uint32_t)(rnd01() * h * 0.012f * SR);
            vel *= 1.0f - rnd01() * h * 0.3f;
        }
        if (FLAM(v)) {                   /* the grace stroke on the step, the stroke just after */
            dr_hit(i, ART(v), vel * FLAM_GRACE, d);
            d += (uint32_t)(FLAM_S * SR);
        }
        dr_hit(i, ART(v), vel, d);
        seq.hits[i]++;
    }
}

static void seq_start(int count_in)
{
    uint32_t i;
    for (i = 0; i < NINST; i++)
        seq.skip[i] = 0;
    seq.step = -1;
    seq.left = 0.0f;
    seq.len = step_len(cur(), 0);
    seq.count = count_in ? nsteps(cur()) : 0u;
    seq.playing = 1;
}

static void seq_stop(void)
{
    seq.playing = 0;
    seq.rec = 0;
    seq.count = 0;
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
        dr_hit(padq.inst[k], padq.art[k], (float)padq.vel[k] * (1.0f / 127.0f), padq.delay[k]);
        seq.hits[padq.inst[k]]++;
        padq.r++;
    }
    while (done < n) {
        uint32_t m = n - done < APP_BLOCK ? n - done : APP_BLOCK;
        if (seq.playing) {               /* the steps that start in these m samples */
            float t = seq.left;
            if ((int32_t)seq.step >= (int32_t)nsteps(cur()))
                seq.step = -1;           /* the bar got shorter */
            while (t < (float)m) {
                seq_next((uint32_t)t);
                t += seq.len;
            }
            seq.left = t - (float)m;
        }
        dr_render(buf, m);
        for (i = 0; i < m; i++) {
            int32_t v;
            gain_now += (gain - gain_now) * 0.004f;
            v = (int32_t)fm_clampf(buf[i] * gain_now, -8300000.0f, 8300000.0f);
            lr[2u * (done + i)] = v;
            lr[2u * (done + i) + 1u] = v;
        }
        done += m;
    }
}

void app_panic(void)
{
    seq_stop();
    dr_panic();
}

/* -------------------------------------------------------------------- text --- */
static struct {
    char msg[2][24];                     /* a short message over the header */
    uint32_t msg_until;
    uint16_t msg_rgb;
    uint32_t keys, btn;
    uint8_t keypage;                     /* the white keys' page of steps */
    uint32_t clear_t;                    /* EDIT once: a second press within 2 s clears the instrument */
    uint32_t tap[4];                     /* SEQ's last taps */
    uint8_t ntap;
    uint32_t hits_seen[NINST], lit_until[NINST];   /* a pad lights when its instrument sounds */
    uint32_t beats_seen, beat_until;
    int32_t drawn_step;
    float bend;                          /* the udu's bass, from its key's pressure: semitones */
    uint8_t redraw;
} ui;

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
    ui.msg_until = plat_ms() + 1300u;
    ui.msg_rgb = 0;
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

static void say_inst(const char *a, const char *b)   /* in the instrument's colour */
{
    app_say(a, b);
    ui.msg_rgb = INST[set.inst].rgb;
}

/* a screen reader that has just started listening (or asks) hears the whole screen */
static void speak(void)
{
    char t[128], n[12];
    if (ann_on() && ann_where()) {
        t[0] = 0;
        text_cat(t, INST[set.inst].name);
        text_cat(t, ". ");
        text_cat(t, RDEF[set.rhythm].name);
        text_cat(t, ", ");
        int_text(cur()->beats, n);
        text_cat(t, n);
        text_cat(t, " BEATS, ");
        int_text(cur()->bpm, n);
        text_cat(t, n);
        text_cat(t, " BPM. ");
        text_cat(t, seq.rec ? "RECORDING. " : seq.playing ? "PLAYING. " : "");
        text_cat(t, PAGE_NAME[set.page]);
        ann_say(t, 0, ANN_NOW);
    }
    ann_tick(plat_ms());
}

/* ---------------------------------------------------------------- changes --- */
static void changed(void)
{
    set_dirty = 1;
    ui.redraw = 1;
}

static void sound_apply(uint32_t i) { dr_inst(i, &set.snd[i]); }

static void inst_step(int32_t d)
{
    set.inst = (uint8_t)((set.inst + NINST + (uint32_t)(d > 0 ? 1 : NINST - 1)) % NINST);
    ui.clear_t = 0;
    changed();
    say_inst(INST[set.inst].name, (cur()->mute >> set.inst & 1u) ? "MUTED" : "");
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

/* a rhythm chosen: a written one comes as it is written (its steps, tempo, swing and strokes: what was changed in it
 * is gone); a FREE one as the player left it. The instrument shown is one it plays. Said: its beats, its name */
static void rhythm_select(uint32_t k)
{
    uint32_t i;
    int any = 0;
    char t[16];
    if (!RDEF[k].free)
        rhythm_default(k);
    set.rhythm = (uint8_t)k;
    ui.keypage = 0;
    for (i = 0; i < NINST; i++)
        any |= has_strokes(cur(), i);
    if (any && !has_strokes(cur(), set.inst))
        for (i = 0; i < NINST; i++)
            if (has_strokes(cur(), i)) {
                set.inst = (uint8_t)i;
                break;
            }
    changed();
    int_text(RDEF[k].beats, t);
    text_cat(t, " BEATS");
    if (!any)
        text_cat(t, ", EMPTY");
    app_say(t, RDEF[k].name);
    ui.msg_until = plat_ms() + RHY_SAY_MS;   /* time to read it while turning on */
}

/* PRESET: the next rhythm with the same beats (round) */
static void rhythm_step(int32_t d)
{
    uint32_t k = set.rhythm, n;
    for (n = 0; n < NRHY; n++) {
        k = (k + (d > 0 ? 1u : NRHY - 1u)) % NRHY;
        if (RDEF[k].beats == RDEF[set.rhythm].beats)
            break;
    }
    if (k != set.rhythm)
        rhythm_select(k);
}

/* ALGORITHM: the beats, 2 to 8: the first rhythm with that many */
static void meter_step(int32_t d)
{
    int32_t b = clamp((int32_t)RDEF[set.rhythm].beats + d, 2, 8);
    uint32_t k;
    if (b == RDEF[set.rhythm].beats)
        return;
    for (k = 0; k < NRHY && RDEF[k].beats != (uint32_t)b; k++)
        ;
    if (k < NRHY)
        rhythm_select(k);
}

static void bpm_set(int32_t v)
{
    char t[12];
    cur()->bpm = (uint8_t)clamp(v, 40, 240);
    changed();
    int_text(cur()->bpm, t);
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
    if (ui.ntap >= 2u && ui.tap[ui.ntap - 1u] - ui.tap[0] >= 100u)
        bpm_set((int32_t)(60000u * (ui.ntap - 1u) + (ui.tap[ui.ntap - 1u] - ui.tap[0]) / 2u) /
                (int32_t)(ui.tap[ui.ntap - 1u] - ui.tap[0]));
    else
        app_say("TAP", "AGAIN");
}

/* -------------------------------------------------------------- the knobs --- */
enum { KN_BPM, KN_DIV, KN_SWING, KN_HUMAN, KN_LEVEL, KN_TUNE, KN_DECAY, KN_BEND, KN_CLICK, KN_COUNT, KN_LIGHT,
       KN_VEL, KN_NONE };
static const uint8_t PAGE_KNOB[NPAGES][4] = {
    {KN_BPM, KN_DIV, KN_SWING, KN_HUMAN},
    {KN_NONE, KN_NONE, KN_NONE, KN_NONE},   /* SOUND: the instrument's own (SOUND_KNOB) */
    {KN_CLICK, KN_COUNT, KN_LIGHT, KN_VEL},
};
/* SOUND: what a player of each can change. The djembe's head is tuned by its ropes and muted by a hand; an udu comes
 * smaller or larger, and the palm over its opening lowers its bass; the maracas shake short or long, the triangle is
 * let ring or held; the cajon and the clapsticks only play louder or softer */
static const uint8_t SOUND_KNOB[NINST][4] = {
    {KN_LEVEL, KN_TUNE, KN_DECAY, KN_NONE},
    {KN_LEVEL, KN_NONE, KN_NONE, KN_NONE},
    {KN_LEVEL, KN_TUNE, KN_BEND, KN_NONE},
    {KN_LEVEL, KN_NONE, KN_NONE, KN_NONE},
    {KN_LEVEL, KN_DECAY, KN_NONE, KN_NONE},
    {KN_LEVEL, KN_DECAY, KN_NONE, KN_NONE},
};
static const char *const KNOB_NAME[KN_NONE] = {"TEMPO", "STEPS", "SWING", "HUMAN", "LEVEL", "TUNE", "DECAY",
                                               "PRESSURE BEND", "CLICK", "COUNT-IN", "LIGHTS", "VELOCITY"};
#define TUNE_MAX 3.0f                    /* semitones either way */

static uint32_t page_knob(uint32_t page, uint32_t k)
{
    return page == PG_SOUND ? SOUND_KNOB[set.inst][k] : PAGE_KNOB[page][k];
}

static void knob_text(uint32_t k, char *t)
{
    dr_inst_t *s = &set.snd[set.inst];
    int32_t v;
    t[0] = 0;
    switch (k) {
    case KN_BPM: int_text(cur()->bpm, t); break;
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
    default: break;
    }
}

static int32_t knob_fill(uint32_t k, int32_t w)   /* a value's bar, 0..w; -1: no bar */
{
    const dr_inst_t *s = &set.snd[set.inst];
    float f;
    switch (k) {
    case KN_BPM: f = (float)(cur()->bpm - 40) / 200.0f; break;
    case KN_SWING: f = (float)cur()->swing * 0.01f; break;
    case KN_HUMAN: f = (float)cur()->human * 0.01f; break;
    case KN_LEVEL: f = s->level; break;
    case KN_TUNE: f = (s->tune + TUNE_MAX) / (2.0f * TUNE_MAX); break;
    case KN_DECAY: f = (s->decay - 0.1f) / 0.9f; break;
    case KN_BEND: f = (float)set.bend / 6.0f; break;
    default: return -1;
    }
    return (int32_t)(fm_clampf(f, 0.0f, 1.0f) * (float)w + 0.5f);
}

static void knob_turn(uint32_t k, int32_t d)
{
    dr_inst_t *s = &set.snd[set.inst];
    char t[16];
    switch (k) {
    case KN_BPM: bpm_set((int32_t)cur()->bpm + d); return;
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
    case KN_BEND: set.bend = (uint8_t)clamp((int32_t)set.bend + d, 0, 6); break;
    case KN_CLICK: set.click = (uint8_t)clamp((int32_t)set.click + (d > 0 ? 1 : -1), CLICK_OFF, CLICK_ON); break;
    case KN_COUNT: set.countin ^= 1u; break;
    case KN_LIGHT: set.lights = (uint8_t)(set.lights == 1u ? 2u : 1u); break;
    case KN_VEL: set.fixed ^= 1u; break;
    default: return;
    }
    sound_apply(set.inst);
    changed();
    knob_text(k, t);
    if (k <= KN_BEND)
        say_inst(KNOB_NAME[k], t);
    else
        app_say(KNOB_NAME[k], t);
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
 * round). During the count-in, only in its last half step: onto the first step */
static void record(uint32_t inst, uint32_t art, uint32_t vel, uint32_t flam)
{
    rhythm_t *r = cur();
    uint32_t f = plat_lock(), ns = nsteps(r), count = seq.count, target, ahead = 0;
    int32_t step = seq.step;
    float left = seq.left, len = seq.len;
    plat_unlock(f);
    if (!seq.rec || !seq.playing)
        return;
    if (count) {
        if (count != 1u || left >= len * 0.5f)
            return;
        target = 0;
        ahead = 1;
    } else if (step < 0) {
        target = 0;
    } else if (left < len * 0.5f) {
        target = (uint32_t)step + 1u >= ns ? 0u : (uint32_t)step + 1u;
        ahead = 1;
    } else {
        target = (uint32_t)step;
    }
    r->step[inst][target] = STEP(art, vel, flam);
    if (ahead)
        seq.skip[inst] = (uint8_t)(target + 1u);
    changed();
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
    if (set.inst == I_DJEMBE && flam_held()) {
        a = ART(v) ? ART(v) : 1u;
        flam = ART(v) ? !FLAM(v) : 1u;
    } else {
        a = ART(v) + 1u;
        if (a > INST[set.inst].nart)
            a = 0;
        flam = FLAM(v);
    }
    r->step[set.inst][s] = a ? STEP(a, key_vel(k), flam) : 0u;
    if (a && !seq.playing)
        pad_play(set.inst, a, key_vel(k), flam);
    ui.clear_t = 0;
    changed();
}

static void pad_key(uint32_t b, uint32_t k)
{
    uint32_t vel = key_vel(k), flam = PAD[b][0] == I_DJEMBE && flam_held();
    pad_play(PAD[b][0], PAD[b][1], vel, flam);
    record(PAD[b][0], PAD[b][1], vel, flam);
}

/* ------------------------------------------------------------------ input --- */
static void button(uint32_t b)
{
    uint32_t i;
    rhythm_t *r = cur();
    for (i = 0; i < NPAGES; i++)
        if (b == PAGE_BTN[i]) {
            set.page = (uint8_t)i;
            ui.redraw = 1;
            return;
        }
    switch (b) {
    case B_PLAY:
        if (seq.playing)
            seq_stop();
        else
            seq_start(0);
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
        say_inst(INST[set.inst].name, (r->mute >> set.inst & 1u) ? "MUTED" : "ON");
        break;
    case B_EDIT:
        if (ui.clear_t && plat_ms() - ui.clear_t < 2000u) {
            for (i = 0; i < NSTEPS; i++)
                r->step[set.inst][i] = 0;
            ui.clear_t = 0;
            changed();
            say_inst(INST[set.inst].name, "CLEARED");
        } else {
            ui.clear_t = plat_ms();
            say_inst("EDIT AGAIN", "TO CLEAR");
        }
        break;
    case B_SEQ:
        tap();
        break;
    case B_ARP:                          /* held: the djembe's flams */
        app_say("FLAM", "ARP + DJEMBE KEY");
        ui.msg_rgb = INST[I_DJEMBE].rgb;
        break;
    case B_OCTDN:
    case B_OCTUP:
        if (npages(r) > 1u) {
            char t[12] = "STEPS ";
            uint32_t p = b == B_OCTUP ? (ui.keypage + 1u < npages(r) ? ui.keypage + 1u : ui.keypage)
                                      : (ui.keypage ? ui.keypage - 1u : 0u);
            ui.keypage = (uint8_t)p;
            int_text((int32_t)(p * page_steps(r) + 1u), t + 6);
            text_cat(t, "+");
            app_say("KEYS", t);
        }
        break;
    case B_SAVE:
        if (plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0 && plat_store_save(OBJ_RHYTHMS, &rhy, RHY_A) == 0
            && plat_store_save(OBJ_RHYTHMS2, &rhy.r[NRHY / 2u], RHY_B) == 0) {
            set_dirty = 0;
            app_say("SAVED", "");
        } else {
            app_say("SAVE", "FAILED");
        }
        break;
    default:
        break;
    }
}

static void input(void)
{
    uint32_t keys, btn = plat_buttons(), k, now = plat_ms(), i;
    uint32_t lit_keys = 0, dim_keys = 0, mid_keys = 0, lit_btn, dim_btn, mid_btn = 0;
    rhythm_t *r = cur();
    int32_t e;
    touch_poll();                        /* the sensed keys' presses and velocities */
    keys = touch_keys();
    for (i = 0; i < 16u; i++)            /* a white key down: a step */
        if ((keys >> WHITE[i] & 1u) && !(ui.keys >> WHITE[i] & 1u))
            step_key(i, WHITE[i]);
    for (i = 0; i < 11u; i++)            /* a black key down: a stroke */
        if ((keys >> BLACK[i] & 1u) && !(ui.keys >> BLACK[i] & 1u))
            pad_key(i, BLACK[i]);
    for (k = 0; k < NB; k++)
        if ((btn >> k & 1u) && !(ui.btn >> k & 1u))
            button(k);
    for (k = 0; k < NE; k++) {
        if ((e = plat_enc((int)k)) == 0)
            continue;
        if (k == EN_SELECT)
            inst_step(e);
        else if (k == EN_ALGO)
            meter_step(e > 0 ? 1 : -1);
        else if (k == EN_PRESET)
            rhythm_step(e > 0 ? 1 : -1);
        else
            knob_turn(page_knob(set.page, k - EN_K1), e);
    }
    {   /* the udu's bass key pressed: its bass lower, as far as the pressure goes (the palm closing the opening) */
        uint32_t uk = BLACK[UDU_BASS_PAD];
        float b = (keys >> uk & 1u) ? -(float)set.bend * (float)touch_pressure(uk) * (1.0f / 127.0f) : 0.0f;
        if (b != ui.bend) {
            ui.bend = b;
            dr_bend(I_UDU, 1, b);
        }
    }
    if (keys != ui.keys)
        ui.redraw = 1;
    ui.keys = keys;
    ui.btn = btn;
    r = cur();
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
    /* the black keys: all dim, the instrument's mid, lit while held or as their instrument sounds */
    for (i = 0; i < NINST; i++)
        if (seq.hits[i] != ui.hits_seen[i]) {
            ui.hits_seen[i] = seq.hits[i];
            ui.lit_until[i] = now + 90u;
        }
    for (i = 0; i < 11u; i++) {
        uint32_t bit = 1u << BLACK[i], in = PAD[i][0];
        if ((keys & bit) || (int32_t)(ui.lit_until[in] - now) > 0)
            lit_keys |= bit;
        else if (in == set.inst)
            mid_keys |= bit;
        else
            dim_keys |= bit;
    }
    if (seq.beats != ui.beats_seen) {    /* SEQ blinks the beat */
        ui.beats_seen = seq.beats;
        ui.beat_until = now + 80u;
    }
    lit_btn = (btn & ~(1u << B_PLAY)) | 1u << PAGE_BTN[set.page] | (set_dirty ? 1u << B_SAVE : 0u);
    if (seq.rec)                         /* PLAY: red recording, green playing (its two LEDs) */
        lit_btn |= 1u << B_PLAY;
    else if (seq.playing || (btn >> B_PLAY & 1u))
        lit_btn |= LED_PLAY_GREEN;
    if (seq.rec && (!seq.count || (now / 150u) & 1u))   /* counting in: REC blinks */
        lit_btn |= 1u << B_REC;
    if (r->mute >> set.inst & 1u)
        lit_btn |= 1u << B_SEL;
    if (ui.clear_t && now - ui.clear_t < 2000u)
        lit_btn |= 1u << B_EDIT;
    if (seq.playing && (int32_t)(ui.beat_until - now) > 0)
        lit_btn |= 1u << B_SEQ;
    if (npages(r) > 1u) {
        if (ui.keypage)
            mid_btn |= 1u << B_OCTDN;
        if (ui.keypage + 1u < npages(r))
            mid_btn |= 1u << B_OCTUP;
    }
    dim_btn = ~lit_btn & ((1u << NB) - 1u) & ~(1u << B_PLAY);
    if (!(lit_btn & (1u << B_PLAY | LED_PLAY_GREEN)))
        dim_btn |= LED_PLAY_GREEN;       /* PLAY glows green */
    plat_lights(lit_btn, lit_keys, dim_btn, dim_keys, set.lights);
    plat_lights_mid(mid_btn, mid_keys);
}

void app_input_only(void) { input(); }

/* ----------------------------------------------------------------- screen --- */
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

/* the header: the instrument (or the page) large in its colour, the six instruments as dots; or a message */
static void header(void)
{
    const rhythm_t *r = cur();
    uint32_t i;
    if (plat_ms() < ui.msg_until) {
        int32_t x = cv_text(8, 6, &FONT_M, ui.msg[0], C_WHITE) + 10;
        const felucca_font_t *f = x + text_w(&FONT_M, ui.msg[1]) > 236 ? &FONT_B : &FONT_M;
        cv_text(x, f == &FONT_M ? 6 : 11, f, ui.msg[1], ui.msg_rgb ? ui.msg_rgb : C_HI);
    } else if (set.page == PG_SETUP) {
        cv_text(8, 2, &FONT_L, "SETUP", C_WHITE);
    } else {
        int muted = (int)(r->mute >> set.inst & 1u);
        cv_text(8, 2, text_w(&FONT_L, INST[set.inst].name) > 150 ? &FONT_M : &FONT_L, INST[set.inst].name,
                muted ? C_DIM : INST[set.inst].rgb);
        if (set.page == PG_HOME)
            for (i = 0; i < NINST; i++) {
                int32_t x = 160 + (int32_t)i * 13;
                uint16_t c = (r->mute >> i & 1u) ? C_DIM : INST[i].rgb;
                if (has_strokes(r, i))
                    cv_rect(x, 10, 10, 10, c);
                else
                    frame(x, 10, 10, 10, 1, c);
                if (i == set.inst)
                    cv_rect(x, 24, 10, 3, C_WHITE);
            }
    }
    cv_rect(0, 36, 240, 1, C_LINE);
}

/* the steps: a row a beat, its steps across; a stroke fills its step in the instrument's colour (dimmer when soft)
 * with its letter; the step sounding has a thick white frame; the rows on the white keys a white mark at the left */
static void draw_steps(void)
{
    const rhythm_t *r = cur();
    const inst_t *in = &INST[set.inst];
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
                cv_rect(x, y, cw, ch, col);
                text_c(x + cw / 2, ty, f, t, C_BLACK);
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

static void footer(void)
{
    const rhythm_t *r = cur();
    char t[16];
    int32_t x;
    int_text(r->bpm, t);
    x = cv_text(8, 204, &FONT_M, t, C_WHITE);
    cv_text(x + 4, 212, &FONT_XS, "BPM", C_GRAY);
    text_c(124, 208, &FONT_B, RDEF[set.rhythm].name, C_GRAY);
    if (seq.count) {                     /* counting in: the beats to go */
        int_text((int32_t)((seq.count + r->div - 1u) / r->div), t);
        cv_text(232 - text_w(&FONT_M, t), 204, &FONT_M, t, C_REC);
    } else if (seq.rec)
        cv_text(232 - text_w(&FONT_M, "REC"), 204, &FONT_M, "REC", C_REC);
    else if (seq.playing)
        cv_text(232 - text_w(&FONT_M, "PLAY"), 204, &FONT_M, "PLAY", C_GO);
}

/* a page of four values, one a row: the name, the value large, a thick bar in the instrument's colour */
static void draw_page(void)
{
    uint32_t i;
    char t[24];
    uint16_t col = set.page == PG_SOUND ? INST[set.inst].rgb : C_HI;
    for (i = 0; i < 4u; i++) {
        uint32_t kn = page_knob(set.page, i);
        int32_t y = 42 + (int32_t)i * 48, f, x;
        if (kn == KN_NONE)
            continue;
        t[0] = 'K';
        t[1] = (char)('1' + i);
        t[2] = 0;
        x = cv_text(8, y + 6, &FONT_XS, t, C_DIM) + 6;
        cv_text(x, y + 4, &FONT_B, KNOB_NAME[kn], C_GRAY);
        knob_text(kn, t);
        cv_text(232 - text_w(&FONT_M, t), y, &FONT_M, t, C_WHITE);
        if ((f = knob_fill(kn, 224)) >= 0) {
            cv_rect(8, y + 28, 224, 12, C_LINE);
            cv_rect(8, y + 28, f, 12, col);
            cv_rect(8 + (f > 3 ? f - 3 : 0), y + 25, 3, 18, C_WHITE);   /* where the value sits */
        }
    }
}

static void draw(void)
{
    int32_t top;
    for (top = 0; top < 240; top += 120) {
        band(top);
        header();
        if (set.page == PG_HOME) {
            draw_steps();
            footer();
        } else
            draw_page();
        cv_rect(0, 235, 240, 5, set.page == PG_SETUP ? C_HI : INST[set.inst].rgb);
        cv_blit(0, (uint32_t)top);
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
        set.snd[i] = INST[i].def;
}

void app_init(void)
{
    uint32_t i;
    int ok = plat_store_load(OBJ_SETTINGS, &set, sizeof set) == (int)sizeof set && set.magic == SET_MAGIC
             && set.rhythm < NRHY && set.inst < NINST && set.page < NPAGES && set.click <= CLICK_ON
             && set.countin <= 1u && set.lights >= 1u && set.lights <= 2u && set.fixed <= 1u && set.bend <= 6u;
    for (i = 0; ok && i < NINST; i++)
        ok = set.snd[i].level >= 0.0f && set.snd[i].level <= 1.0f && set.snd[i].tune >= -TUNE_MAX
             && set.snd[i].tune <= TUNE_MAX && set.snd[i].decay >= 0.1f && set.snd[i].decay <= 1.0f;
    if (!ok)
        settings_default();
    ok = plat_store_load(OBJ_RHYTHMS, &rhy, RHY_A) == (int)RHY_A && rhy.magic == RHY_MAGIC
         && plat_store_load(OBJ_RHYTHMS2, &rhy.r[NRHY / 2u], RHY_B) == (int)RHY_B;
    for (i = 0; ok && i < NRHY; i++)
        ok = rhy.r[i].beats == RDEF[i].beats && rhy.r[i].div >= 2u && rhy.r[i].div <= 4u
             && rhy.r[i].bpm >= 40u && rhy.r[i].bpm <= 240u && rhy.r[i].swing <= 100u && rhy.r[i].human <= 100u;
    if (!ok) {
        rhy.magic = RHY_MAGIC;
        for (i = 0; i < NRHY; i++)
            rhythm_default(i);
    }
    seq.rnd = 0x2545F491u;
    dr_init(SR);
    for (i = 0; i < NINST; i++)
        sound_apply(i);
}

void app_ui_init(void)
{
    touch_on(1);                         /* velocity: the keys' readings on */
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.drawn_step = -2;
    ui.redraw = 1;
}

void app_frame(void)
{
    uint32_t now = plat_ms();
    static uint32_t last_count;
    input();
    speak();
    if (ui.msg_until && now >= ui.msg_until) {
        ui.msg_until = 0;
        ui.redraw = 1;
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
    else if (!strcmp(what, "swing"))
        *got = r->swing;
    else if (!strcmp(what, "human"))
        *got = r->human;
    else if (!strcmp(what, "playing"))
        *got = seq.playing;
    else if (!strcmp(what, "rec"))
        *got = seq.rec;
    else if (!strcmp(what, "count"))
        *got = (int)seq.count;
    else if (!strcmp(what, "step"))
        *got = seq.step;
    else if (!strcmp(what, "page"))
        *got = set.page;
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
    else if (!strcmp(what, "tune"))      /* the instrument's tuning, tenths of a semitone */
        *got = (int)(set.snd[set.inst].tune * 10.0f + (set.snd[set.inst].tune < 0.0f ? -0.5f : 0.5f));
    else if (what[0] == 'v' && what[1] >= '0' && what[1] <= '9')   /* vN: its velocity */
        *got = ART(r->step[set.inst][atoi(what + 1) % NSTEPS]) ? (int)VEL(r->step[set.inst][atoi(what + 1) % NSTEPS]) : 0;
    else if (!strncmp(what, "hits", 4))  /* hitsI: instrument I's strokes played since boot */
        *got = (int)seq.hits[atoi(what + 4) % NINST];
    else if (!strcmp(what, "msg"))       /* a message is on the screen */
        *got = ui.msg_until && (int32_t)(ui.msg_until - plat_ms()) > 0;
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
