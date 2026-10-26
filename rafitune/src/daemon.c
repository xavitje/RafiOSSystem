/*
 * RafiTune – daemon.c
 * De hoofdlus: elke paar seconden kijken of er een game draait.
 *
 *   geen game  ──(game start)──>  game actief  ──(game stopt)──>  geen game
 *                                 - CPU op performance             - alles terugzetten
 *                                 - game meer voorrang
 *                                 - achtergrondtaken afremmen
 */

#include "daemon.h"

#include "algemeen.h"
#include "cpufreq.h"
#include "prioriteit.h"
#include "proces.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t stoppen = 0;

static void bij_signaal(int sig)
{
    (void)sig;
    stoppen = 1;
}

struct staat {
    int actief;
    char sleutel[96];
    char naam[96];
    char herkend[96];
    time_t sinds;
    struct profiel profiel;
    int cpu_aantal;
    int game_n;
    int afrem_n;
};

static struct cpufreq_staat cpu; /* groot, dus niet op de stack */

/* De game met de betrouwbaarste herkenning wint (config > Steam > .exe) */
static struct proces *kies_game(struct proces_lijst *l)
{
    struct proces *beste = NULL;
    for (size_t i = 0; i < l->aantal; i++) {
        struct proces *p = &l->p[i];
        if (p->klasse == KLASSE_GAME && (beste == NULL || p->bron < beste->bron)) {
            beste = p;
        }
    }
    return beste;
}

static void start_game(struct staat *st, const struct config *c, const struct proces *p)
{
    proces_sleutel(p, st->sleutel, sizeof(st->sleutel));
    proces_game_naam(p, c, st->naam, sizeof(st->naam));
    proces_herkend(p, st->herkend, sizeof(st->herkend));
    st->profiel = config_profiel(c, p->bron == BRON_CONFIG ? &c->games[p->game] : NULL);
    st->sinds = time(NULL);
    st->actief = 1;

    log_info("game gestart: %s (herkend via %s)", st->naam, st->herkend);
    log_info("profiel: prioriteit %d, cpu %s, afremmen %s", st->profiel.prioriteit,
             st->profiel.cpu_performance ? "performance" : "ongewijzigd",
             st->profiel.afremmen ? "ja" : "nee");

    st->cpu_aantal = 0;
    if (st->profiel.cpu_performance) {
        st->cpu_aantal = cpufreq_performance(&cpu);
        if (st->cpu_aantal > 0 && !proef_modus) {
            log_info("CPU op performance (%d policies)", st->cpu_aantal);
        }
    }
}

static void stop_game(struct staat *st, struct proces_lijst *l)
{
    int teruggezet = 0;
    for (size_t i = 0; i < l->aantal; i++) {
        struct proces *p = &l->p[i];
        if (p->aangepast || p->io_aangepast) {
            prioriteit_herstel(p);
            teruggezet++;
        }
    }
    if (teruggezet > 0) {
        log_info("%s%d proces(sen) teruggezet naar de oude voorrang",
                 proef_modus ? "PROEF: " : "", teruggezet);
    }
    cpufreq_herstel(&cpu);

    long duur = (long)(time(NULL) - st->sinds);
    log_info("game gestopt: %s (na %ld min %ld s)", st->naam, duur / 60, duur % 60);
    st->actief = 0;
    st->sleutel[0] = st->naam[0] = st->herkend[0] = '\0';
    st->cpu_aantal = 0;
}

static void pas_toe(struct staat *st, struct proces_lijst *l)
{
    int nieuw_game = 0;
    int nieuw_afrem = 0;
    char namen[256] = "";
    st->game_n = st->afrem_n = 0;

    for (size_t i = 0; i < l->aantal; i++) {
        struct proces *p = &l->p[i];
        int was = p->aangepast;

        if (p->klasse == KLASSE_GAME) {
            st->game_n++;
            if (st->profiel.prioriteit != 0) {
                prioriteit_game(p, st->profiel.prioriteit);
                nieuw_game += (!was && p->aangepast);
            }
        } else if (p->klasse == KLASSE_AFREMMEN && st->profiel.afremmen) {
            st->afrem_n++;
            prioriteit_afremmen(p);
            if (!was && p->aangepast) {
                nieuw_afrem++;
                size_t lengte = strlen(namen);
                snprintf(namen + lengte, sizeof(namen) - lengte, "%s%s", lengte ? ", " : "",
                         p->naam);
            }
        }
    }

    /* Elke controle worden ook nieuwe threads meegenomen; we loggen alleen nieuwe processen */
    if (nieuw_game > 0) {
        log_info("%svoorrang %d gegeven aan %d game-proces(sen)", proef_modus ? "PROEF: zou " : "",
                 st->profiel.prioriteit, nieuw_game);
    }
    if (nieuw_afrem > 0) {
        log_info("%safgeremd: %s", proef_modus ? "PROEF: zou " : "", namen);
    }
}

static void schrijf_status(const struct staat *st)
{
    static char laatste[1024];
    char tekst[1024];
    char tijd[32] = "-";
    char profiel[96] = "-";

    if (st->actief) {
        struct tm t;
        localtime_r(&st->sinds, &t);
        strftime(tijd, sizeof(tijd), "%Y-%m-%d %H:%M:%S", &t);
        snprintf(profiel, sizeof(profiel), "prioriteit %d, cpu %s, afremmen %s",
                 st->profiel.prioriteit,
                 st->profiel.cpu_performance ? "performance" : "ongewijzigd",
                 st->profiel.afremmen ? "ja" : "nee");
    }
    snprintf(tekst, sizeof(tekst),
             "actief=%s\ngame=%s\nherkend=%s\nsinds=%s\nprofiel=%s\n"
             "game_processen=%d\nafgeremd=%d\ncpu_policies=%d\nproef=%s\n",
             st->actief ? "ja" : "nee", st->actief ? st->naam : "-",
             st->actief ? st->herkend : "-", tijd, profiel, st->game_n, st->afrem_n,
             st->cpu_aantal, proef_modus ? "ja" : "nee");

    if (strcmp(tekst, laatste) == 0) {
        return; /* niets veranderd: niet onnodig schrijven */
    }
    char map[512];
    char pad[512];
    if (pad_maak(map, sizeof(map), "/run/rafitune") != 0 ||
        pad_maak(pad, sizeof(pad), "/run/rafitune/status") != 0) {
        return;
    }
    maak_map(map);
    if (schrijf_veilig(pad, tekst) == 0) {
        snprintf(laatste, sizeof(laatste), "%s", tekst);
    }
}

static void slaap(double seconden)
{
    struct timespec t;
    t.tv_sec = (time_t)seconden;
    t.tv_nsec = (long)((seconden - (double)t.tv_sec) * 1e9);
    nanosleep(&t, NULL); /* een signaal maakt hem eerder wakker: prima */
}

int daemon_draai(const struct config *c, long max_keer)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = bij_signaal;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    signal(SIGHUP, SIG_IGN);

    log_info("gestart%s: interval %.1f s, %d game(s) in de instellingen",
             proef_modus ? " in PROEF-modus (verandert niets)" : "", c->interval,
             c->aantal_games);
    if (!pad_test() && !proef_modus && geteuid() != 0) {
        log_info("let op: niet gestart als root, dus voorrang geven en de CPU aanpassen zal "
                 "mislukken. Gebruik: sudo systemctl start rafitune");
    }

    cpufreq_herstel_na_crash();

    struct proces_lijst lijst = {0};
    struct staat st;
    memset(&st, 0, sizeof(st));
    int resultaat = 0;

    for (long keer = 0; !stoppen && (max_keer < 0 || keer < max_keer); keer++) {
        if (keer > 0) {
            slaap(c->interval);
            if (stoppen) {
                break;
            }
        }
        if (proces_scan(&lijst, c) != 0) {
            log_fout("kan /proc niet lezen: %s", strerror(errno));
            resultaat = 1;
            break;
        }

        struct proces *game = kies_game(&lijst);
        char sleutel[96] = "";
        if (game != NULL) {
            proces_sleutel(game, sleutel, sizeof(sleutel));
        }

        if (st.actief && (game == NULL || strcmp(sleutel, st.sleutel) != 0)) {
            stop_game(&st, &lijst); /* game gestopt of een andere game gestart */
        }
        if (game != NULL && !st.actief) {
            start_game(&st, c, game);
        }
        if (st.actief) {
            pas_toe(&st, &lijst);
        } else {
            st.game_n = st.afrem_n = 0;
        }
        schrijf_status(&st);
    }

    if (st.actief) {
        stop_game(&st, &lijst);
    }
    schrijf_status(&st);
    proces_lijst_vrij(&lijst);
    log_info("gestopt");
    return resultaat;
}

int daemon_scan_toon(const struct config *c)
{
    struct proces_lijst lijst = {0};
    if (proces_scan(&lijst, c) != 0) {
        log_fout("kan /proc niet lezen");
        return 1;
    }

    int games = 0;
    int afrem = 0;
    printf("%-8s %-16s %-10s %s\n", "PID", "NAAM", "SOORT", "HOORT BIJ");
    for (size_t i = 0; i < lijst.aantal; i++) {
        const struct proces *p = &lijst.p[i];
        if (p->klasse == KLASSE_GAME) {
            char naam[96];
            char herkend[96];
            proces_game_naam(p, c, naam, sizeof(naam));
            proces_herkend(p, herkend, sizeof(herkend));
            printf("%-8d %-16s %-10s %s (via %s)\n", p->pid, p->naam, "game", naam, herkend);
            games++;
        } else if (p->klasse == KLASSE_AFREMMEN) {
            printf("%-8d %-16s %-10s %s\n", p->pid, p->naam, "afremmen", "achtergrondtaak");
            afrem++;
        }
    }
    struct proces *gekozen = kies_game(&lijst);
    if (gekozen != NULL) {
        char naam[96];
        proces_game_naam(gekozen, c, naam, sizeof(naam));
        printf("\nRafiTune zou nu gamen met: %s (%d game-processen, %d om af te remmen)\n", naam,
               games, afrem);
    } else {
        printf("\nGeen game gevonden (%d achtergrondtaken die afgeremd zouden worden).\n",
               afrem);
    }
    if (geteuid() != 0 && !pad_test()) {
        printf("Tip: als gewone gebruiker zie je alleen je eigen processen goed; "
               "probeer sudo rafitune scan.\n");
    }
    proces_lijst_vrij(&lijst);
    return 0;
}

int daemon_status_toon(void)
{
    char pad[512];
    char buf[1024];
    if (pad_maak(pad, sizeof(pad), "/run/rafitune/status") != 0 ||
        lees_bestand(pad, buf, sizeof(buf)) < 0) {
        printf("RafiTune draait niet (geen statusbestand).\n");
        printf("Starten: sudo systemctl start rafitune\n");
        return 3;
    }

    static const char *labels[][2] = {
        {"actief", "Game actief"},       {"game", "Game"},
        {"herkend", "Herkend via"},      {"sinds", "Sinds"},
        {"profiel", "Profiel"},          {"game_processen", "Game-processen"},
        {"afgeremd", "Afgeremd"},        {"cpu_policies", "CPU aangepast"},
        {"proef", "Proefmodus"},
    };
    printf("RafiTune\n");
    char *rest = buf;
    char *regel;
    while ((regel = strsep(&rest, "\n")) != NULL) {
        char *is = strchr(regel, '=');
        if (is == NULL) {
            continue;
        }
        *is = '\0';
        const char *label = regel;
        for (size_t i = 0; i < sizeof(labels) / sizeof(labels[0]); i++) {
            if (!strcmp(regel, labels[i][0])) {
                label = labels[i][1];
            }
        }
        printf("  %-16s %s\n", label, is + 1);
    }
    return 0;
}
