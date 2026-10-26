/*
 * RafiTune – cpufreq.h
 * CPU op volle snelheid zetten tijdens het gamen, en daarna netjes terug.
 */

#ifndef RAFITUNE_CPUFREQ_H
#define RAFITUNE_CPUFREQ_H

#define CPUFREQ_MAX 256

/* Per "policy" (meestal per kern of per groep kernen) */
struct cpufreq_policy {
    char naam[32];      /* bijv. "policy0" */
    char oud_gov[32];   /* governor van voor de game */
    char gezet_gov[32]; /* wat wij erin zetten ("" = niets veranderd) */
    char oud_epp[48];   /* energy_performance_preference van voor de game ("" = bestaat niet) */
    int epp_gezet;      /* 1 = wij hebben EPP veranderd */
};

struct cpufreq_staat {
    int actief;
    int aantal;
    struct cpufreq_policy p[CPUFREQ_MAX];
};

/* CPU op "performance". Geeft het aantal aangepaste policies terug. */
int cpufreq_performance(struct cpufreq_staat *s);

/* Zet alles terug, maar alleen als het nog op onze waarde staat.
 * Heeft iets anders (GameMode, de gebruiker) het al veranderd, dan blijven we eraf. */
void cpufreq_herstel(struct cpufreq_staat *s);

/* Na een crash: oude waarden uit /run/rafitune/cpu-origineel terugzetten. */
void cpufreq_herstel_na_crash(void);

#endif
