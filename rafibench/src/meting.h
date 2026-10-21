/*
 * RafiBench – meting.h
 * Een MangoHud-logbestand inlezen en er cijfers van maken.
 */

#ifndef RAFIBENCH_METING_H
#define RAFIBENCH_METING_H

#include <stddef.h>

/* Alle frametijden (in milliseconden) uit één logbestand */
struct frames {
    double *ms;      /* lijst met frametijden */
    size_t aantal;   /* hoeveel er in de lijst staan */
    size_t ruimte;   /* hoeveel er passen voordat we moeten vergroten */
};

/* De uitkomst van een benchmark */
struct resultaat {
    size_t frames;
    double seconden;        /* totale gemeten tijd */
    double fps_gemiddeld;   /* frames / tijd */
    double fps_1_low;       /* gemiddelde fps van de traagste 1% frames */
    double fps_01_low;      /* gemiddelde fps van de traagste 0,1% frames */
    double ms_mediaan;      /* frametijd in het midden */
    double ms_p99;          /* 99% van de frames is sneller dan dit */
    double ms_slechtst;     /* langzaamste frame */
    size_t haperingen;      /* frames langer dan 2x de mediaan */
};

/* Leest een MangoHud-CSV. Geeft 0 terug als het lukt, -1 bij een fout. */
int frames_lees(const char *pad, struct frames *f);

/* Geeft het geheugen van de lijst weer vrij. */
void frames_vrij(struct frames *f);

/* Rekent de resultaten uit. Let op: sorteert f->ms. Geeft -1 bij te weinig data. */
int frames_analyseer(struct frames *f, struct resultaat *r);

#endif
