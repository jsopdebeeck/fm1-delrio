/* SPDX-License-Identifier: MPL-2.0 */
/* FM-1_DelRio's rhythms as written (app.c includes it once, after the instruments and rhythm_t): the kit's, then the
 * berimbau's toques, each with its source. */

/* the rhythms at first: one string an instrument, a character a step: the stroke's letter (capital: strong, small:
 * soft), "F" its last stroke (the slap) as a flam, "." none. From the literature:
 *   MARCHA, VALSA, MAZURCA: Santo Daime's three rhythms, as its maracá plays them (f soft, F strong): marcha f f f F,
 *     valsa F F f, mazurca (6/8) f f f F F F (Jornal Grande Bahia, "Abaixo o maracá. Viva o maracá", 2016)
 *   IJEXA, BARRAVENTO: Umbanda's atabaque toques on the djembe, Tum its bass, Ta its tone, the marking stroke (M)
 *     strong: Ijexá "Ta TaTa Tum Ta", Barravento (6/8) "Tum TaTa TumTum" (Templo Tata Possum, Curso de Atabaque, 2024);
 *     IJEXA's agogo in sixteenths, high and low, "H.H.L.L.HH.H.LL." (Gilson de Assis, Brazilian Conga, Advance Music
 *     2006, ch. 3 "Ijexá", Agogo 1; the same in P. Meirelles's teaching book, UFRJ PROMUS 2024)
 *   BAIAO: the zabumba on the first beat's first and fourth sixteenths (here the djembe's bass), the triangle's
 *     sixteenths closed, open on the off-beats (ANPPOM 2018); SAMBA: the surdo soft on 1, strong on 2 (the cajon's
 *     bass), the ganzá's sixteenths (the maracas)
 *   IJEXA CANTO, XOTE, MARCHA 2/4, TOADA, and BAIAO's djembe: the grooves the urban neo-ayahuasca circles put on the
 *     djembe under their cantos (practical adaptations, not canonical transcriptions; the Huni Kuin's own cantos have
 *     the voice, a maracá at most): IJEXA CANTO for the slow, swaying ones, BAIAO or MARCHA 2/4 for the ones that
 *     dance, TOADA for the ternary ones
 *   Each instrument's own: KUKU, the djembe's accompaniment "B.TT..S.B.TT..S." (M. Gasewicz, "A rhythm notation for
 *     djembe and dundun drums", BachoTeX 2023); LANDO, the Afro-Peruvian cajon's "B . F . B B | . F B . F B" in 6/4,
 *     F a flam (Wikipedia, "Landó (music)"; Thomann's cajon guide); BULERIAS, the flamenco cajon's slaps on the
 *     compás's accents, 3 6 8 10 12 of 12 (K. Kocoshis, Percussive Notes 2025); JOROPO, the llanero maracas' six
 *     eighths accented as 6/8 (Radio Nacional de Colombia, "Joropo, más allá de un ritmo de las llanuras")
 *   Brazil's on the djembe, as djembegorilla.com's library has them (its grids of sixteenths; its muted tones and
 *     taps soft tones here): AFOXE (two bars), AFRO SAMBA, BOSSA NOVA, FUNK IJEXA, SAMBA DJEMBE, XOTE DJEMBE
 *   West Africa's, from Paul Nas's box notations ("West African Percussions"): DJOLE, the Temine's mask dance (its
 *     third djembe part; the kenkeni's bell on the clapsticks, the dundunba on the cajon), YANKADI, the Sousou's
 *     slow dance (its first djembe part, the dundunba's bell on every pulse) */
typedef struct {
    const char *name;
    uint8_t beats, div, bpm, swing;
    uint8_t free;                        /* the player's own: kept as left, not written again when chosen */
    const char *pat[NROW];               /* a row each (ROW_INST): the kit's djembe, cajon, udu, clapsticks, maracas,
                                          * triangle, agogo; the berimbau's medio, viola, pandeiro, atabaque, agogo,
                                          * reco-reco, gunga, caxixi (none: 0) */
} rdef_t;
/* grouped by their beats (ALGORITHM), each group ending with its FREE one (PRESET goes round a group) */
static const rdef_t RDEF[] = {
    {"HEARTBEAT", 2, 4, 64, 0, 0, {"Bb..Bb..", "", "B.......", "", "..x...x.", ""}},
    {"BAIAO", 2, 4, 100, 0, 0, {"B..B..S.", "", "", "", "", "MMOMMMOM"}},
    {"XOTE", 2, 4, 84, 0, 0, {"B..BS...", "", "", "", "", ""}},
    {"SAMBA", 2, 4, 100, 0, 0, {"", "b...B...", "", "", "xxxXxxxX", ""}},
    {"MARCHA 2/4", 2, 4, 108, 0, 0, {"B.T.B.T.", "", "", "", "", ""}},
    {"MAZURCA", 2, 3, 60, 0, 0, {"", "", "", "", "xxxXXX", ""}},
    {"BARRAVENTO", 2, 3, 72, 0, 0, {"B.ttbb", "", "", "", "", ""}},
    {"JOROPO", 2, 3, 132, 0, 0, {"", "", "", "", "XxxXxx", ""}},
    {"FREE", 2, 4, 90, 0, 1, {"", "", "", "", "", ""}},
    {"THREE", 3, 4, 84, 0, 0, {"", "B...S...S...", "", "", "X.x.X.x.X.x.", "O..........."}},
    {"VALSA", 3, 2, 120, 0, 0, {"", "", "", "", "X.X.x.", ""}},
    {"TOADA", 3, 2, 100, 0, 0, {"B.T.T.", "", "", "", "", ""}},
    {"FREE", 3, 4, 90, 0, 1, {"", "", "", "", "", ""}},
    {"FOUR", 4, 4, 92, 0, 0, {"", "B...S...B.b.S...", "", "", "x.X.x.X.x.X.x.X.", "O.......M......."}},
    {"JOURNEY", 4, 2, 116, 0, 0, {"BbbbBbbb", "", "B.......", "", "", ""}},
    {"SHAKER", 4, 4, 76, 0, 0, {"", "", "B.......b.......", "", "XxxxXxxxXxxxXxxx", "O..............."}},
    {"CLAVE", 4, 4, 100, 0, 0, {"....T.T.....T.TS", "B.......B.......", "", "X..X..X...X.X...", "x.x.x.x.x.x.x.x.", ""}},
    {"AFRO 12/8", 4, 3, 100, 0, 0, {"S..TT.S..TT.", "", "B.....b.....", "X.X.XX.X.X.X", "X.xX.xX.xX.x", ""}},
    {"MARCHA", 4, 2, 120, 0, 0, {"", "", "", "", "x.x.x.X.", ""}},
    {"IJEXA", 4, 4, 96, 0, 0, {"T...t.t.B...t...", "", "", "", "x...x...x...x...", "", "H.H.L.L.HH.H.LL."}},
    {"IJEXA CANTO", 4, 4, 88, 0, 0, {"B.T.S.TTB.T.S...", "", "", "", "", ""}},
    {"KUKU", 4, 4, 120, 0, 0, {"B.TT..S.B.TT..S.", "", "", "", "", ""}},
    {"AFRO SAMBA", 4, 4, 80, 0, 0, {"BTTTBtBtBTTTBBBB", "", "", "", "", ""}},
    {"BOSSA NOVA", 4, 4, 80, 0, 0, {"TttTBtTBBtTBBTtB", "", "", "", "", ""}},
    {"FUNK IJEXA", 4, 4, 95, 0, 0, {"BBBSS.T.BBBSS.TT", "", "", "", "", ""}},
    {"SAMBA DJEMBE", 4, 4, 100, 0, 0, {"BT.TB.TBB.TBBT.T", "", "", "", "", ""}},
    {"XOTE DJEMBE", 4, 4, 80, 0, 0, {"TTSST.BSTTSST.B.", "", "", "", "", ""}},
    {"DJOLE", 4, 4, 120, 0, 0, {"T.SSB.SSTTSSB.SS", "B.......B.......", "", "..XX..XX..XX..XX", "", ""}},
    {"YANKADI", 4, 4, 90, 0, 0, {"B.TTBSS.B.T.BSS.", "", "", "xxxxxxxxxxxxxxxx", "", ""}},
    {"FREE", 4, 4, 90, 0, 1, {"", "", "", "", "", ""}},
    {"FIVE", 5, 2, 110, 0, 0, {"", "B.s.s.B.S.", "", "", "XxXxXxXxXx", "O........."}},
    {"FREE", 5, 2, 90, 0, 1, {"", "", "", "", "", ""}},
    {"LANDO", 6, 2, 96, 0, 0, {"", "B.F.BB.FB.FB", "", "", "", ""}},
    {"BULERIAS", 6, 2, 108, 0, 0, {"", "..S..S.S.S.S", "", "", "", ""}},
    {"FREE", 6, 2, 90, 0, 1, {"", "", "", "", "", ""}},
    {"SEVEN", 7, 2, 150, 0, 0, {"", "B...S...B...S.", "", "X...X...X.....", "x.x.x.x.x.x.x.", ""}},
    {"FREE", 7, 2, 90, 0, 1, {"", "", "", "", "", ""}},
    {"AFOXE", 8, 4, 115, 0, 0, {"B..T..B.T..T..B.T..T..B.TT.T..B.", "", "", "", "", ""}},
    {"FREE", 8, 2, 90, 0, 1, {"", "", "", "", "", ""}},
    /* the berimbau's toques, as the Cordao de Ouro teaches them ("Toques do berimbau", Flensburg 2020), counted
     * "0 - 1 - 2 - 3": the two chiados (TSCH, soft) on 0, the free DOM (O) and the pressed DIM (I) on the counts;
     * the caxixi in the pause, where the source shakes it. Each count an eighth of the toque's 2/4 bar (its BPM
     * here, the bar's quarter half of it), a quarter slower than a roda plays them, for the ceremony: Angola's
     * quarter 45, Sao Bento Grande's 66. SB REGIONAL, Mestre Bimba's, over two counts: "tsch tsch DOM, tsch
     * DIM, tsch tsch DOM DOM DIM". And from opencapoeira.com's "Toques de berimbau" (its | a chiado, || two): JOGO
     * DE FORA, Jogo de Dentro turned over; CAVALARIA whole, "|| DOM | DOM || DOM DIM DOM" (the Cordao de Ouro's
     * its last count); IDALINA "|| DOM DOM DIM DOM DOM | DIM"; MIUDINHO "|| DIM | DIM | DIM DOM DIM" */
    {"ANGOLA", 4, 2, 90, 0, 0, {[RB_GUNGA] = "xxO.I...", [RB_CAXIXI] = "......x."}},
    {"SB PEQUENO", 4, 2, 104, 0, 0, {[RB_GUNGA] = "xxI.O...", [RB_CAXIXI] = "......x."}},
    {"SB GRANDE", 4, 2, 132, 0, 0, {[RB_GUNGA] = "xxI.O.O."}},
    {"BANGUELA", 4, 2, 96, 0, 0, {[RB_GUNGA] = "xxO.I.I."}},
    {"JOGO DE DENTRO", 4, 2, 100, 0, 0, {[RB_GUNGA] = "xxI.O.I."}},
    {"JOGO DE FORA", 4, 2, 100, 0, 0, {[RB_GUNGA] = "xxO.I.O."}},
    {"FREE", 4, 2, 90, 0, 1, {0}},
    {"SB REGIONAL", 8, 2, 136, 0, 0, {[RB_GUNGA] = "xxO.x.I.xxO.O.I."}},
    {"CAVALARIA", 8, 2, 138, 0, 0, {[RB_GUNGA] = "xxO.x.O.xxO.I.O."}},
    {"IDALINA", 8, 2, 112, 0, 0, {[RB_GUNGA] = "xxO.O.I.O.O.x.I."}},
    {"MIUDINHO", 8, 2, 90, 0, 0, {[RB_GUNGA] = "xxI.x.I.x.I.O.I."}},
    {"FREE", 8, 2, 90, 0, 1, {0}},
    /* the roda of capoeira Angola, its bateria whole (after 0.9's places: appended). The three berimbaus as CPPA's
     * "Toques de Berimbau" (2023) and Capoeira Baoba (2026) give them: the gunga ANGOLA, the medio SAO BENTO PEQUENO
     * (Angola turned over), the viola SAO BENTO GRANDE de Angola (varied: VIRADAS); counted as above, 0 the chiados.
     * The pandeiro (its thumb T, its slap P) and the atabaque (its bass T, its open tone P) as Elias's "So Ritmo"
     * (Grupo Capoeira Brasil, 2017) writes their "Capoeira" grooves, moved onto the chiados' count; the agogo (L, H)
     * and the reco-reco as Wikipedia's "Capoeira music" has them (uncited there) */
    {"RODA ANGOLA", 4, 2, 90, 0, 0, {[RB_GUNGA] = "xxO.I...", [RB_CAXIXI] = "......x.", [RB_MEDIO] = "xxI.O...",
                                     [RB_VIOLA] = "xxI.O.O.", [RB_PANDEIRO] = ".TT.P.T.", [RB_ATABAQUE] = ".TT.P.T.",
                                     [RB_AGOGO] = "..L.H.L.", [RB_RECO] = "..R.R.R."}},
};
