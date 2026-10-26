/*
 * RafiTune – config.c
 *
 * Voorbeeld van het bestand:
 *
 *   [algemeen]
 *   interval = 2
 *   prioriteit = -5
 *
 *   [game Counter-Strike 2]
 *   herken = appid:730
 *   prioriteit = -10
 */

#include "config.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Processen die meestal niet nodig zijn tijdens het gamen.
 * Let op: de kernel kapt procesnamen af op 15 tekens. */
static const char *standaard_afremmen[] = {
    "baloo_file",      "baloo_file_extractor", "tracker-miner-fs-3",
    "packagekitd",     "rpm-ostree",           "rpm-ostreed",
    "uupd",            "flatpak",              "fwupd",
    "dnf5",            "localsearch-3",        NULL,
};

/* Windows-programma's die geen game zijn (Wine/Proton-onderdelen en launchers) */
static const char *standaard_negeer[] = {
    "steam.exe",        "steamwebhelper.exe", "explorer.exe",    "services.exe",
    "winedevice.exe",   "plugplay.exe",       "svchost.exe",     "rpcss.exe",
    "conhost.exe",      "start.exe",          "tabtip.exe",      "wineboot.exe",
    "winemenubuilder.exe", "rundll32.exe",    "msiexec.exe",     "cmd.exe",
    "iexplore.exe",     "regedit.exe",        "notepad.exe",     "setup.exe",
    "install.exe",      "unins000.exe",       "epicgameslauncher.exe",
    "epicwebhelper.exe", "battle.net.exe",    "eadesktop.exe",   "ubisoftconnect.exe",
    "upc.exe",          "crashhandler.exe",   "unitycrashhandler64.exe",
    "easyanticheat.exe", "easyanticheat_eos_setup.exe", NULL,
};

#include "algemeen.h"

#define kopieer kopieer_tekst

static void naar_kleine_letters(char *s)
{
    for (; *s; s++) {
        *s = (char)tolower((unsigned char)*s);
    }
}

void config_standaard(struct config *c)
{
    memset(c, 0, sizeof(*c));
    c->interval = 2.0;
    c->lees_omgeving = 1;
    c->herken_steam = 1;
    c->herken_exe = 1;
    c->standaard.prioriteit = -5;
    c->standaard.cpu_performance = 1;
    c->standaard.afremmen = 1;

    for (int i = 0; standaard_afremmen[i] && c->aantal_afremmen < MAX_NAMEN; i++) {
        kopieer(c->afremmen[c->aantal_afremmen++], sizeof(c->afremmen[0]),
                standaard_afremmen[i]);
    }
    for (int i = 0; standaard_negeer[i] && c->aantal_negeer < MAX_NAMEN; i++) {
        kopieer(c->negeer_exe[c->aantal_negeer++], sizeof(c->negeer_exe[0]),
                standaard_negeer[i]);
    }
}

int naam_past(const char *comm, const char *gezocht)
{
    /* Linux bewaart maar 15 tekens van een procesnaam */
    size_t lengte = strlen(gezocht);
    if (lengte > 15) {
        return strlen(comm) == 15 && strncmp(comm, gezocht, 15) == 0;
    }
    return strcmp(comm, gezocht) == 0;
}

static char *trim(char *s)
{
    while (isspace((unsigned char)*s)) {
        s++;
    }
    char *eind = s + strlen(s);
    while (eind > s && isspace((unsigned char)eind[-1])) {
        *--eind = '\0';
    }
    return s;
}

static int lees_ja_nee(const char *w, int *uit)
{
    if (!strcasecmp(w, "ja") || !strcasecmp(w, "aan") || !strcmp(w, "1") ||
        !strcasecmp(w, "true") || !strcasecmp(w, "yes")) {
        *uit = 1;
        return 0;
    }
    if (!strcasecmp(w, "nee") || !strcasecmp(w, "uit") || !strcmp(w, "0") ||
        !strcasecmp(w, "false") || !strcasecmp(w, "no")) {
        *uit = 0;
        return 0;
    }
    return -1;
}

static int lees_getal(const char *w, long min, long max, int *uit)
{
    char *eind;
    long v = strtol(w, &eind, 10);
    if (eind == w || *eind != '\0' || v < min || v > max) {
        return -1;
    }
    *uit = (int)v;
    return 0;
}

/* Splitst "a, b, c" in een lijst. Geeft -1 als de lijst te lang is. */
static int lees_lijst(char *w, char *lijst, size_t breedte, int max, int *aantal, int klein)
{
    *aantal = 0;
    char *rest = w;
    char *stuk;
    while ((stuk = strsep(&rest, ",")) != NULL) {
        stuk = trim(stuk);
        if (*stuk == '\0') {
            continue;
        }
        if (*aantal >= max) {
            return -1;
        }
        char *doel = lijst + (size_t)(*aantal) * breedte;
        kopieer(doel, breedte, stuk);
        if (klein) {
            naar_kleine_letters(doel);
        }
        (*aantal)++;
    }
    return 0;
}

static int lees_profiel_sleutel(const char *sleutel, const char *waarde, struct profiel *p,
                                int *bekend)
{
    *bekend = 1;
    if (!strcmp(sleutel, "prioriteit")) {
        return lees_getal(waarde, -20, 19, &p->prioriteit);
    }
    if (!strcmp(sleutel, "cpu_performance")) {
        return lees_ja_nee(waarde, &p->cpu_performance);
    }
    if (!strcmp(sleutel, "afremmen")) {
        return lees_ja_nee(waarde, &p->afremmen);
    }
    *bekend = 0;
    return 0;
}

static int lees_herken(char *waarde, struct herken *h)
{
    char *dubbelepunt = strchr(waarde, ':');
    if (dubbelepunt == NULL) {
        return -1;
    }
    *dubbelepunt = '\0';
    char *soort = trim(waarde);
    char *wat = trim(dubbelepunt + 1);
    if (*wat == '\0') {
        return -1;
    }

    if (!strcmp(soort, "appid")) {
        for (const char *p = wat; *p; p++) {
            if (!isdigit((unsigned char)*p)) {
                return -1;
            }
        }
        h->soort = HERKEN_APPID;
    } else if (!strcmp(soort, "naam")) {
        h->soort = HERKEN_NAAM;
    } else if (!strcmp(soort, "exe")) {
        h->soort = HERKEN_EXE;
    } else if (!strcmp(soort, "cmdline")) {
        h->soort = HERKEN_CMDLINE;
    } else {
        return -1;
    }
    kopieer(h->waarde, sizeof(h->waarde), wat);
    if (h->soort == HERKEN_EXE) {
        naar_kleine_letters(h->waarde);
    }
    return 0;
}

int config_lees(const char *pad, struct config *c, FILE *fouten)
{
    FILE *f = fopen(pad, "r");
    if (f == NULL) {
        return -1;
    }

    enum { GEEN, ALGEMEEN, GAME, ONBEKEND } sectie = GEEN;
    struct game *g = NULL;
    int aantal_fouten = 0;
    int regelnr = 0;
    char regel[1024];

#define FOUT(...)                                              \
    do {                                                       \
        fprintf(fouten, "%s:%d: ", pad, regelnr);              \
        fprintf(fouten, __VA_ARGS__);                          \
        fputc('\n', fouten);                                   \
        aantal_fouten++;                                       \
    } while (0)

    while (fgets(regel, sizeof(regel), f) != NULL) {
        regelnr++;
        if (strchr(regel, '\n') == NULL && !feof(f)) {
            FOUT("regel is te lang (max %zu tekens)", sizeof(regel) - 2);
            int t;
            while ((t = fgetc(f)) != EOF && t != '\n') {
            }
            continue;
        }
        char *hekje = strchr(regel, '#');
        if (hekje) {
            *hekje = '\0';
        }
        char *r = trim(regel);
        if (*r == '\0' || *r == ';') {
            continue;
        }

        /* Nieuwe sectie: [algemeen] of [game Naam] */
        if (*r == '[') {
            char *sluit = strchr(r, ']');
            if (sluit == NULL || sluit[1] != '\0') {
                FOUT("sectie moet eindigen met ]");
                sectie = ONBEKEND;
                continue;
            }
            *sluit = '\0';
            char *naam = trim(r + 1);
            if (!strcmp(naam, "algemeen")) {
                sectie = ALGEMEEN;
            } else if (!strncmp(naam, "game", 4) && isspace((unsigned char)naam[4])) {
                if (c->aantal_games >= MAX_GAMES) {
                    FOUT("te veel games (max %d)", MAX_GAMES);
                    sectie = ONBEKEND;
                    continue;
                }
                g = &c->games[c->aantal_games++];
                memset(g, 0, sizeof(*g));
                kopieer(g->naam, sizeof(g->naam), trim(naam + 4));
                g->profiel.prioriteit = NIET_GEZET;
                g->profiel.cpu_performance = -1;
                g->profiel.afremmen = -1;
                sectie = GAME;
            } else {
                FOUT("onbekende sectie [%s] (gebruik [algemeen] of [game Naam])", naam);
                sectie = ONBEKEND;
            }
            continue;
        }

        char *is = strchr(r, '=');
        if (is == NULL) {
            FOUT("verwacht 'sleutel = waarde'");
            continue;
        }
        *is = '\0';
        char *sleutel = trim(r);
        char *waarde = trim(is + 1);

        if (sectie == GEEN) {
            FOUT("'%s' staat niet in een sectie; begin met [algemeen]", sleutel);
            continue;
        }
        if (sectie == ONBEKEND) {
            continue; /* fout is al gemeld bij de sectie */
        }

        int bekend;
        if (sectie == ALGEMEEN) {
            if (lees_profiel_sleutel(sleutel, waarde, &c->standaard, &bekend) != 0) {
                FOUT("ongeldige waarde '%s' voor %s", waarde, sleutel);
            } else if (bekend) {
                /* al verwerkt */
            } else if (!strcmp(sleutel, "interval")) {
                char *eind;
                double v = strtod(waarde, &eind);
                if (eind == waarde || *eind != '\0' || v < 0.1 || v > 60) {
                    FOUT("interval moet tussen 0.1 en 60 seconden liggen");
                } else {
                    c->interval = v;
                }
            } else if (!strcmp(sleutel, "lees_omgeving") || !strcmp(sleutel, "herken_steam") ||
                       !strcmp(sleutel, "herken_exe")) {
                int *doel = !strcmp(sleutel, "lees_omgeving") ? &c->lees_omgeving
                            : !strcmp(sleutel, "herken_steam") ? &c->herken_steam
                                                               : &c->herken_exe;
                if (lees_ja_nee(waarde, doel) != 0) {
                    FOUT("%s moet ja of nee zijn", sleutel);
                }
            } else if (!strcmp(sleutel, "afremmen_processen")) {
                if (lees_lijst(waarde, &c->afremmen[0][0], sizeof(c->afremmen[0]), MAX_NAMEN,
                               &c->aantal_afremmen, 0) != 0) {
                    FOUT("te veel namen in afremmen_processen (max %d)", MAX_NAMEN);
                }
            } else if (!strcmp(sleutel, "negeer_exe")) {
                /* Komt bovenop de standaardlijst */
                char extra[MAX_NAMEN][64];
                int n;
                if (lees_lijst(waarde, &extra[0][0], sizeof(extra[0]), MAX_NAMEN, &n, 1) != 0 ||
                    c->aantal_negeer + n > MAX_NAMEN) {
                    FOUT("te veel namen in negeer_exe (max %d in totaal)", MAX_NAMEN);
                } else {
                    for (int i = 0; i < n; i++) {
                        kopieer(c->negeer_exe[c->aantal_negeer++], sizeof(c->negeer_exe[0]),
                                extra[i]);
                    }
                }
            } else {
                FOUT("onbekende sleutel '%s' in [algemeen]", sleutel);
            }
        } else { /* GAME */
            if (lees_profiel_sleutel(sleutel, waarde, &g->profiel, &bekend) != 0) {
                FOUT("ongeldige waarde '%s' voor %s", waarde, sleutel);
            } else if (bekend) {
                /* al verwerkt */
            } else if (!strcmp(sleutel, "herken")) {
                if (g->aantal_regels >= MAX_REGELS) {
                    FOUT("te veel herken-regels voor '%s' (max %d)", g->naam, MAX_REGELS);
                } else if (lees_herken(waarde, &g->regels[g->aantal_regels]) != 0) {
                    FOUT("herken moet zijn: appid:<getal>, naam:<proces>, exe:<bestand.exe> "
                         "of cmdline:<tekst>");
                } else {
                    g->aantal_regels++;
                }
            } else {
                FOUT("onbekende sleutel '%s' in [game %s]", sleutel, g->naam);
            }
        }
    }
    fclose(f);

    for (int i = 0; i < c->aantal_games; i++) {
        if (c->games[i].aantal_regels == 0) {
            fprintf(fouten, "%s: [game %s] heeft geen 'herken = ...'-regel\n", pad,
                    c->games[i].naam);
            aantal_fouten++;
        }
    }
#undef FOUT
    return aantal_fouten;
}

struct profiel config_profiel(const struct config *c, const struct game *g)
{
    struct profiel p = c->standaard;
    if (g != NULL) {
        if (g->profiel.prioriteit != NIET_GEZET) {
            p.prioriteit = g->profiel.prioriteit;
        }
        if (g->profiel.cpu_performance >= 0) {
            p.cpu_performance = g->profiel.cpu_performance;
        }
        if (g->profiel.afremmen >= 0) {
            p.afremmen = g->profiel.afremmen;
        }
    }
    return p;
}

static void toon_profiel(const struct profiel *p, FILE *uit)
{
    fprintf(uit, "prioriteit %d, cpu %s, afremmen %s", p->prioriteit,
            p->cpu_performance ? "performance" : "ongewijzigd", p->afremmen ? "ja" : "nee");
}

void config_toon(const struct config *c, FILE *uit)
{
    static const char *soorten[] = {"appid", "naam", "exe", "cmdline"};

    fprintf(uit, "Algemeen\n");
    fprintf(uit, "  interval        %.1f s\n", c->interval);
    fprintf(uit, "  herken Steam    %s%s\n", c->herken_steam ? "ja" : "nee",
            c->lees_omgeving ? "" : " (uit: lees_omgeving = nee)");
    fprintf(uit, "  herken .exe     %s\n", c->herken_exe ? "ja" : "nee");
    fprintf(uit, "  standaard       ");
    toon_profiel(&c->standaard, uit);
    fprintf(uit, "\n  afremmen        ");
    for (int i = 0; i < c->aantal_afremmen; i++) {
        fprintf(uit, "%s%s", i ? ", " : "", c->afremmen[i]);
    }
    fprintf(uit, "\n  negeer .exe     %d namen\n", c->aantal_negeer);

    fprintf(uit, "\nGames (%d)\n", c->aantal_games);
    for (int i = 0; i < c->aantal_games; i++) {
        const struct game *g = &c->games[i];
        struct profiel p = config_profiel(c, g);
        fprintf(uit, "  %s\n    herken:  ", g->naam);
        for (int k = 0; k < g->aantal_regels; k++) {
            fprintf(uit, "%s%s:%s", k ? ", " : "", soorten[g->regels[k].soort],
                    g->regels[k].waarde);
        }
        fprintf(uit, "\n    profiel: ");
        toon_profiel(&p, uit);
        fputc('\n', uit);
    }
}
