/*
 * RafiTune – cpu.c
 * Leest het CPU-gebruik uit /proc/stat.
 *
 * Hoe het werkt:
 * De kernel houdt per CPU-kern bij hoeveel tijd ("ticks") die besteed heeft
 * aan verschillende dingen: user, nice, system, idle, iowait, ...
 * Die getallen lopen alleen maar op. Door twee keer te meten en het verschil
 * te nemen, weet je hoeveel procent van de tijd de CPU bezig was.
 */

#include "cpu.h"

#include <stdio.h>
#include <string.h>

int cpu_lees(struct cpu_meting *meting)
{
    FILE *f = fopen("/proc/stat", "r");
    if (f == NULL) {
        perror("rafitune: kan /proc/stat niet openen");
        return -1;
    }

    char regel[512];
    meting->aantal_kernen = 0;
    int totaal_gevonden = 0;

    while (fgets(regel, sizeof(regel), f) != NULL) {
        /* We willen alleen regels die beginnen met "cpu" */
        if (strncmp(regel, "cpu", 3) != 0) {
            continue;
        }

        unsigned long long user = 0, nice = 0, system = 0, idle = 0;
        unsigned long long iowait = 0, irq = 0, softirq = 0, steal = 0;
        char naam[16];

        int velden = sscanf(regel, "%15s %llu %llu %llu %llu %llu %llu %llu %llu",
                            naam, &user, &nice, &system, &idle,
                            &iowait, &irq, &softirq, &steal);
        if (velden < 5) {
            continue; /* regel klopt niet, overslaan */
        }

        struct cpu_tijden t;
        t.niets_doen = idle + iowait;
        t.totaal = user + nice + system + idle + iowait + irq + softirq + steal;

        if (strcmp(naam, "cpu") == 0) {
            /* De eerste regel "cpu" is alle kernen samen */
            meting->totaal = t;
            totaal_gevonden = 1;
        } else if (meting->aantal_kernen < CPU_MAX_KERNEN) {
            /* "cpu0", "cpu1", ... zijn de losse kernen */
            meting->kern[meting->aantal_kernen] = t;
            meting->aantal_kernen++;
        }
    }

    fclose(f);

    if (!totaal_gevonden) {
        fprintf(stderr, "rafitune: geen cpu-regel gevonden in /proc/stat\n");
        return -1;
    }
    return 0;
}

double cpu_gebruik(const struct cpu_tijden *voor, const struct cpu_tijden *na)
{
    /* Verschil tussen de twee metingen */
    unsigned long long totaal = na->totaal - voor->totaal;
    unsigned long long niets = na->niets_doen - voor->niets_doen;

    if (totaal == 0) {
        return 0.0; /* geen tijd verstreken, voorkom delen door 0 */
    }
    if (niets > totaal) {
        niets = totaal;
    }
    return 100.0 * (double)(totaal - niets) / (double)totaal;
}
