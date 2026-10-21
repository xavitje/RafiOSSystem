/*
 * RafiTune – main.c
 * Eerste versie: laat elke seconde het CPU-gebruik zien.
 *
 * Gebruik:
 *   rafitune          blijft meten tot je Ctrl+C drukt
 *   rafitune -n 5     meet 5 keer en stopt dan
 *   rafitune -k       laat ook elke kern apart zien
 */

#include "cpu.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t stoppen = 0;

static void bij_ctrl_c(int sig)
{
    (void)sig;
    stoppen = 1;
}

static void teken_balk(double procent)
{
    /* Een balkje van 20 tekens: elk teken is 5% */
    int vol = (int)(procent / 5.0 + 0.5);
    putchar('[');
    for (int i = 0; i < 20; i++) {
        putchar(i < vol ? '#' : '.');
    }
    putchar(']');
}

static void gebruik(const char *naam)
{
    printf("Gebruik: %s [-n aantal] [-k]\n", naam);
    printf("  -n aantal  aantal metingen (standaard: oneindig)\n");
    printf("  -k         ook het gebruik per kern laten zien\n");
}

int main(int argc, char **argv)
{
    long aantal = -1; /* -1 = oneindig */
    int per_kern = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            char *eind;
            aantal = strtol(argv[++i], &eind, 10);
            if (*eind != '\0' || aantal <= 0) {
                fprintf(stderr, "rafitune: -n wil een getal groter dan 0\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-k") == 0) {
            per_kern = 1;
        } else {
            gebruik(argv[0]);
            return strcmp(argv[i], "-h") == 0 ? 0 : 1;
        }
    }

    signal(SIGINT, bij_ctrl_c);

    /* static: de struct is groot (256 kernen), dus niet op de stack */
    static struct cpu_meting voor, na;
    if (cpu_lees(&voor) != 0) {
        return 1;
    }

    printf("RafiTune – CPU-gebruik (Ctrl+C om te stoppen)\n");

    for (long n = 0; !stoppen && (aantal < 0 || n < aantal); n++) {
        sleep(1);
        if (stoppen) {
            break;
        }
        if (cpu_lees(&na) != 0) {
            return 1;
        }

        double totaal = cpu_gebruik(&voor.totaal, &na.totaal);
        printf("CPU totaal %5.1f%% ", totaal);
        teken_balk(totaal);
        putchar('\n');

        if (per_kern) {
            int kernen = na.aantal_kernen < voor.aantal_kernen
                             ? na.aantal_kernen
                             : voor.aantal_kernen;
            for (int k = 0; k < kernen; k++) {
                double p = cpu_gebruik(&voor.kern[k], &na.kern[k]);
                printf("  kern %3d %5.1f%% ", k, p);
                teken_balk(p);
                putchar('\n');
            }
        }
        fflush(stdout);

        voor = na; /* de nieuwe meting wordt de oude voor de volgende ronde */
    }

    return 0;
}
