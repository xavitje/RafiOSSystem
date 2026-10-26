/*
 * RafiTune – main.c
 * Het performance-programma van RafiOS.
 *
 *   rafitune daemon         draait op de achtergrond (via systemd)
 *   rafitune status         wat doet RafiTune nu?
 *   rafitune scan           welke games en achtergrondtaken ziet RafiTune?
 *   rafitune test-config    instellingenbestand controleren
 *   rafitune cpu            CPU-gebruik live bekijken
 */

#include "algemeen.h"
#include "config.h"
#include "cpu.h"
#include "daemon.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define STANDAARD_CONFIG "/etc/rafitune/rafitune.conf"

static void gebruik(FILE *uit)
{
    fprintf(uit,
            "RafiTune – performance voor RafiOS\n"
            "\n"
            "Gebruik: rafitune <opdracht> [opties]\n"
            "\n"
            "Opdrachten:\n"
            "  daemon        op de achtergrond games herkennen en optimaliseren\n"
            "  status        laten zien wat RafiTune nu doet\n"
            "  scan          laten zien welke games/achtergrondtaken RafiTune ziet\n"
            "  test-config   het instellingenbestand controleren\n"
            "  cpu           CPU-gebruik live bekijken (-n aantal, -k per kern)\n"
            "\n"
            "Opties:\n"
            "  -c <bestand>  ander instellingenbestand (standaard %s)\n"
            "  --proef       alleen laten zien wat er zou gebeuren (daemon)\n"
            "  --keer <n>    stoppen na n controles (daemon, voor tests)\n",
            STANDAARD_CONFIG);
}

/* Leest de instellingen. Geeft het aantal fouten, of -1 als het bestand verplicht was. */
static int laad_config(const char *pad, int zelf_gekozen, struct config *c)
{
    config_standaard(c);
    int fouten = config_lees(pad, c, stderr);
    if (fouten < 0) {
        if (zelf_gekozen) {
            log_fout("kan %s niet openen", pad);
            return -1;
        }
        log_info("%s niet gevonden, standaardinstellingen gebruikt", pad);
        return 0;
    }
    return fouten;
}

/* ---- rafitune cpu (de eerste versie van RafiTune) ---- */

static volatile sig_atomic_t cpu_stoppen = 0;

static void cpu_ctrl_c(int sig)
{
    (void)sig;
    cpu_stoppen = 1;
}

static void teken_balk(double procent)
{
    int vol = (int)(procent / 5.0 + 0.5);
    putchar('[');
    for (int i = 0; i < 20; i++) {
        putchar(i < vol ? '#' : '.');
    }
    putchar(']');
}

static int opdracht_cpu(long aantal, int per_kern)
{
    signal(SIGINT, cpu_ctrl_c);
    static struct cpu_meting voor, na;
    if (cpu_lees(&voor) != 0) {
        return 1;
    }
    printf("RafiTune – CPU-gebruik (Ctrl+C om te stoppen)\n");
    for (long n = 0; !cpu_stoppen && (aantal < 0 || n < aantal); n++) {
        sleep(1);
        if (cpu_stoppen) {
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
            int kernen = na.aantal_kernen < voor.aantal_kernen ? na.aantal_kernen
                                                               : voor.aantal_kernen;
            for (int k = 0; k < kernen; k++) {
                double p = cpu_gebruik(&voor.kern[k], &na.kern[k]);
                printf("  kern %3d %5.1f%% ", k, p);
                teken_balk(p);
                putchar('\n');
            }
        }
        fflush(stdout);
        voor = na;
    }
    return 0;
}

/* ---- main ---- */

int main(int argc, char **argv)
{
    pad_init();

    if (argc < 2 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help") ||
        !strcmp(argv[1], "help")) {
        gebruik(stdout);
        return argc < 2 ? 1 : 0;
    }
    const char *opdracht = argv[1];

    const char *config_pad = STANDAARD_CONFIG;
    int zelf_gekozen = 0;
    long keer = -1;
    long aantal = -1;
    int per_kern = 0;

    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "-c") && i + 1 < argc) {
            config_pad = argv[++i];
            zelf_gekozen = 1;
        } else if (!strcmp(argv[i], "--proef")) {
            proef_modus = 1;
        } else if ((!strcmp(argv[i], "--keer") || !strcmp(argv[i], "-n")) && i + 1 < argc) {
            char *eind;
            long v = strtol(argv[++i], &eind, 10);
            if (*eind != '\0' || v <= 0) {
                log_fout("%s wil een getal groter dan 0", argv[i - 1]);
                return 1;
            }
            if (!strcmp(argv[i - 1], "--keer")) {
                keer = v;
            } else {
                aantal = v;
            }
        } else if (!strcmp(argv[i], "-k")) {
            per_kern = 1;
        } else {
            log_fout("onbekende optie '%s'", argv[i]);
            gebruik(stderr);
            return 1;
        }
    }

    if (!strcmp(opdracht, "cpu")) {
        return opdracht_cpu(aantal, per_kern);
    }
    if (!strcmp(opdracht, "status")) {
        return daemon_status_toon();
    }

    /* De rest heeft instellingen nodig. struct config is groot: op de heap. */
    struct config *c = malloc(sizeof(*c));
    if (c == NULL) {
        log_fout("te weinig geheugen");
        return 1;
    }
    int fouten = laad_config(config_pad, zelf_gekozen, c);
    int code = 0;

    if (!strcmp(opdracht, "test-config")) {
        if (fouten < 0) {
            code = 1;
        } else {
            config_toon(c, stdout);
            if (fouten > 0) {
                printf("\n%d fout(en) gevonden; die regels worden overgeslagen.\n", fouten);
                code = 1;
            } else {
                printf("\nInstellingen zijn in orde.\n");
            }
        }
    } else if (!strcmp(opdracht, "scan")) {
        code = fouten < 0 ? 1 : daemon_scan_toon(c);
    } else if (!strcmp(opdracht, "daemon")) {
        if (fouten < 0) {
            code = 1;
        } else {
            if (fouten > 0) {
                log_fout("%d fout(en) in %s; die regels worden overgeslagen "
                         "(controleer met: rafitune test-config)",
                         fouten, config_pad);
            }
            code = daemon_draai(c, keer);
        }
    } else {
        log_fout("onbekende opdracht '%s'", opdracht);
        gebruik(stderr);
        code = 1;
    }

    free(c);
    return code;
}
