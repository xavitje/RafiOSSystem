/*
 * RafiTune – cpu.h
 * Functies om het CPU-gebruik te meten.
 */

#ifndef RAFITUNE_CPU_H
#define RAFITUNE_CPU_H

/* Meer kernen dan dit worden genegeerd */
#define CPU_MAX_KERNEN 256

/* Tijden van één CPU (of alle CPU's samen) op één moment */
struct cpu_tijden {
    unsigned long long totaal;      /* alle tijd bij elkaar */
    unsigned long long niets_doen;  /* idle + wachten op schijf */
};

/* Eén complete meting van /proc/stat */
struct cpu_meting {
    struct cpu_tijden totaal;               /* alle kernen samen */
    struct cpu_tijden kern[CPU_MAX_KERNEN]; /* per kern */
    int aantal_kernen;
};

/* Leest /proc/stat in. Geeft 0 terug als het lukt, -1 bij een fout. */
int cpu_lees(struct cpu_meting *meting);

/* Rekent het gebruik in procent (0-100) uit tussen twee metingen. */
double cpu_gebruik(const struct cpu_tijden *voor, const struct cpu_tijden *na);

#endif
