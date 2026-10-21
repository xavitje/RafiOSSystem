/*
 * RafiBench – main.c
 * Analyseert MangoHud-logs.
 *
 * Gebruik:
 *   rafibench-analyse log.csv                     resultaten laten zien
 *   rafibench-analyse --vorige oud.csv log.csv    vergelijken met een eerdere meting
 *   rafibench-analyse --tsv log.csv               één regel voor een tabel (voor scripts)
 */

#include "meting.h"

#include <stdio.h>
#include <string.h>

static int meet(const char *pad, struct resultaat *r)
{
    struct frames f;
    if (frames_lees(pad, &f) != 0) {
        return -1;
    }
    int ok = frames_analyseer(&f, r);
    frames_vrij(&f);
    return ok;
}

/* Verschil in procent, met + of - ervoor */
static void verschil(double oud, double nieuw)
{
    if (oud <= 0.0) {
        printf("%10s", "");
        return;
    }
    printf("%+9.1f%%", 100.0 * (nieuw - oud) / oud);
}

static void regel_fps(const char *naam, double nieuw, const double *oud)
{
    printf("  %-22s %9.1f", naam, nieuw);
    if (oud != NULL) {
        printf("  (was %7.1f) ", *oud);
        verschil(*oud, nieuw);
    }
    putchar('\n');
}

static void toon(const struct resultaat *r, const struct resultaat *oud)
{
    printf("  %-22s %9zu   in %.1f s\n", "Frames", r->frames, r->seconden);
    regel_fps("Gemiddelde fps", r->fps_gemiddeld, oud ? &oud->fps_gemiddeld : NULL);
    regel_fps("1% low (fps)", r->fps_1_low, oud ? &oud->fps_1_low : NULL);
    regel_fps("0,1% low (fps)", r->fps_01_low, oud ? &oud->fps_01_low : NULL);
    printf("  %-22s %9.2f ms\n", "Frametijd mediaan", r->ms_mediaan);
    printf("  %-22s %9.2f ms\n", "Frametijd 99%", r->ms_p99);
    printf("  %-22s %9.2f ms\n", "Traagste frame", r->ms_slechtst);
    printf("  %-22s %9zu   (frames > 2x mediaan)\n", "Haperingen", r->haperingen);

    if (oud != NULL) {
        /* Hogere 1% low = soepeler spel; dat telt het zwaarst */
        double winst = 100.0 * (r->fps_1_low - oud->fps_1_low) / oud->fps_1_low;
        printf("\n  Conclusie: ");
        if (winst > 3.0) {
            printf("soepeler dan de vorige meting (+%.1f%% 1%% low).\n", winst);
        } else if (winst < -3.0) {
            printf("minder soepel dan de vorige meting (%.1f%% 1%% low).\n", winst);
        } else {
            printf("geen duidelijk verschil (binnen 3%%, kan meetruis zijn).\n");
        }
    }
}

static void gebruik(void)
{
    printf("Gebruik:\n");
    printf("  rafibench-analyse <log.csv>\n");
    printf("  rafibench-analyse --vorige <oud.csv> <log.csv>\n");
    printf("  rafibench-analyse --tsv <log.csv>\n");
}

int main(int argc, char **argv)
{
    const char *vorige = NULL;
    const char *log = NULL;
    int tsv = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--vorige") == 0 && i + 1 < argc) {
            vorige = argv[++i];
        } else if (strcmp(argv[i], "--tsv") == 0) {
            tsv = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            gebruik();
            return 0;
        } else if (log == NULL && argv[i][0] != '-') {
            log = argv[i];
        } else {
            gebruik();
            return 1;
        }
    }
    if (log == NULL) {
        gebruik();
        return 1;
    }

    struct resultaat nu;
    if (meet(log, &nu) != 0) {
        return 1;
    }

    if (tsv) {
        /* frames, seconden, gemiddeld, 1% low, 0,1% low */
        printf("%zu\t%.1f\t%.1f\t%.1f\t%.1f\n", nu.frames, nu.seconden,
               nu.fps_gemiddeld, nu.fps_1_low, nu.fps_01_low);
        return 0;
    }

    struct resultaat oud;
    int heeft_oud = 0;
    if (vorige != NULL) {
        if (meet(vorige, &oud) == 0) {
            heeft_oud = 1;
        } else {
            fprintf(stderr, "rafibench: vorige meting overgeslagen\n");
        }
    }

    toon(&nu, heeft_oud ? &oud : NULL);
    return 0;
}
