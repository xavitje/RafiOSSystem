/*
 * RafiBench – meting.c
 *
 * Een MangoHud-log ziet er ongeveer zo uit:
 *
 *   os,cpu,gpu,ram,kernel,driver,cpuscheduler      <- systeeminfo (kop)
 *   Fedora,Ryzen 5 5600,RX 6700 XT,...              <- systeeminfo (waarden)
 *   fps,frametime,cpu_load,gpu_load,...,elapsed     <- kop van de meetdata
 *   143.2,6.98,35,97,...                            <- één regel per frame
 *
 * We zoeken de regel met een kolom "frametime", onthouden welke kolom dat is,
 * en lezen daarna van elke regel alleen die kolom.
 */

#include "meting.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Zoekt in een kopregel in welke kolom (0, 1, 2, ...) 'naam' staat. -1 = niet gevonden */
static int zoek_kolom(char *regel, const char *naam)
{
    int kolom = 0;
    char *rest = regel;
    char *veld;

    while ((veld = strsep(&rest, ",")) != NULL) {
        veld[strcspn(veld, "\r\n")] = '\0'; /* einde-regel weghalen */
        if (strcmp(veld, naam) == 0) {
            return kolom;
        }
        kolom++;
    }
    return -1;
}

/* Haalt het getal uit kolom 'kolom'. Geeft 0 terug als het lukt. */
static int lees_kolom(const char *regel, int kolom, double *waarde)
{
    const char *p = regel;
    for (int i = 0; i < kolom; i++) {
        p = strchr(p, ',');
        if (p == NULL) {
            return -1; /* regel heeft te weinig kolommen */
        }
        p++;
    }
    char *eind;
    errno = 0;
    double w = strtod(p, &eind);
    if (eind == p || errno != 0) {
        return -1; /* geen getal */
    }
    *waarde = w;
    return 0;
}

static int frames_voeg_toe(struct frames *f, double ms)
{
    if (f->aantal == f->ruimte) {
        /* Lijst is vol: twee keer zo groot maken */
        size_t nieuw = f->ruimte ? f->ruimte * 2 : 4096;
        double *groter = realloc(f->ms, nieuw * sizeof(double));
        if (groter == NULL) {
            return -1;
        }
        f->ms = groter;
        f->ruimte = nieuw;
    }
    f->ms[f->aantal++] = ms;
    return 0;
}

int frames_lees(const char *pad, struct frames *f)
{
    FILE *bestand = fopen(pad, "r");
    if (bestand == NULL) {
        fprintf(stderr, "rafibench: kan '%s' niet openen: %s\n", pad, strerror(errno));
        return -1;
    }

    memset(f, 0, sizeof(*f));
    char regel[4096];
    int kolom = -1;
    int fout = 0;

    while (fgets(regel, sizeof(regel), bestand) != NULL) {
        if (kolom < 0) {
            /* Nog geen kop gevonden: is dit hem? (zoek_kolom verandert de regel) */
            char kopie[sizeof(regel)];
            memcpy(kopie, regel, sizeof(kopie));
            kolom = zoek_kolom(kopie, "frametime");
            continue;
        }
        double ms;
        if (lees_kolom(regel, kolom, &ms) != 0 || ms <= 0.0) {
            continue; /* kapotte of lege regel overslaan */
        }
        if (frames_voeg_toe(f, ms) != 0) {
            fprintf(stderr, "rafibench: te weinig geheugen\n");
            fout = 1;
            break;
        }
    }
    fclose(bestand);

    if (fout) {
        frames_vrij(f);
        return -1;
    }
    if (kolom < 0) {
        fprintf(stderr, "rafibench: '%s' is geen MangoHud-log (geen kolom 'frametime')\n", pad);
        return -1;
    }
    return 0;
}

void frames_vrij(struct frames *f)
{
    free(f->ms);
    f->ms = NULL;
    f->aantal = f->ruimte = 0;
}

/* Voor qsort: grootste frametijd (traagste frame) eerst */
static int traagste_eerst(const void *a, const void *b)
{
    double x = *(const double *)a;
    double y = *(const double *)b;
    return (x < y) - (x > y);
}

/* Gemiddelde fps van de traagste 'deel' (bijv. 0.01 = 1%) van de frames */
static double fps_low(const double *traag_eerst, size_t n, double deel)
{
    size_t hoeveel = (size_t)((double)n * deel);
    if (hoeveel < 1) {
        hoeveel = 1;
    }
    double som = 0.0;
    for (size_t i = 0; i < hoeveel; i++) {
        som += traag_eerst[i];
    }
    return 1000.0 / (som / (double)hoeveel);
}

int frames_analyseer(struct frames *f, struct resultaat *r)
{
    if (f->aantal < 10) {
        fprintf(stderr, "rafibench: te weinig frames (%zu) voor een betrouwbare meting\n",
                f->aantal);
        return -1;
    }

    size_t n = f->aantal;
    double totaal_ms = 0.0;
    for (size_t i = 0; i < n; i++) {
        totaal_ms += f->ms[i];
    }

    qsort(f->ms, n, sizeof(double), traagste_eerst);

    memset(r, 0, sizeof(*r));
    r->frames = n;
    r->seconden = totaal_ms / 1000.0;
    r->fps_gemiddeld = 1000.0 * (double)n / totaal_ms;
    r->fps_1_low = fps_low(f->ms, n, 0.01);
    r->fps_01_low = fps_low(f->ms, n, 0.001);
    r->ms_mediaan = f->ms[n / 2];
    r->ms_p99 = f->ms[n / 100];
    r->ms_slechtst = f->ms[0];

    for (size_t i = 0; i < n && f->ms[i] > 2.0 * r->ms_mediaan; i++) {
        r->haperingen++;
    }
    return 0;
}
