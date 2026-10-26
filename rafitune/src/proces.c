/*
 * RafiTune – proces.c
 *
 * Voor elk proces heeft Linux een map /proc/<pid> met o.a.:
 *   stat     naam, status en starttijd
 *   cmdline  de opdrachtregel (stukken gescheiden door NUL-tekens)
 *   environ  de omgevingsvariabelen; Steam zet daar SteamAppId=<nummer> in
 *
 * Snel blijven: elke controle lezen we alleen /proc/<pid>/stat. Alleen voor
 * een NIEUW proces (of een proces dat van naam verandert) lezen we cmdline en
 * environ. Zo kost RafiTune bijna niets, ook met honderden processen.
 */

#include "proces.h"

#include "algemeen.h"

#include <ctype.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

/* Leest naam en starttijd uit /proc/<pid>/stat. 0 = gelukt. */
static int lees_stat(int pid, char naam[16], unsigned long long *start)
{
    char pad[512];
    char buf[1024];
    if (pad_maak(pad, sizeof(pad), "/proc/%d/stat", pid) != 0 ||
        lees_bestand(pad, buf, sizeof(buf)) < 0) {
        return -1;
    }
    /* Formaat: "pid (naam) S ppid ...". De naam kan spaties of haakjes bevatten,
     * dus we zoeken het eerste '(' en het LAATSTE ')'. */
    char *open = strchr(buf, '(');
    char *dicht = strrchr(buf, ')');
    if (open == NULL || dicht == NULL || dicht < open) {
        return -1;
    }
    size_t lengte = (size_t)(dicht - open - 1);
    if (lengte > 15) {
        lengte = 15;
    }
    memcpy(naam, open + 1, lengte);
    naam[lengte] = '\0';

    /* Na ')' komt veld 3. De starttijd is veld 22, dus het 20e veld hierna. */
    char *rest = dicht + 1;
    char *stuk = NULL;
    char *bewaar = NULL;
    for (int veld = 3; veld <= 22; veld++) {
        stuk = strtok_r(veld == 3 ? rest : NULL, " ", &bewaar);
        if (stuk == NULL) {
            return -1;
        }
    }
    *start = strtoull(stuk, NULL, 10);
    return 0;
}

/* Zoekt "NAAM=" in een environ-blok (NUL-gescheiden). Geeft de waarde of NULL. */
static const char *zoek_omgeving(const char *blok, int lengte, const char *naam)
{
    size_t n = strlen(naam);
    const char *p = blok;
    const char *eind = blok + lengte;
    while (p < eind) {
        size_t stuk = strnlen(p, (size_t)(eind - p));
        if (stuk > n && strncmp(p, naam, n) == 0 && p[n] == '=') {
            return p + n + 1;
        }
        p += stuk + 1;
    }
    return NULL;
}

/* Bestandsnaam van een pad, met / of \ (Windows-paden onder Wine) */
static const char *basisnaam(const char *pad)
{
    const char *a = strrchr(pad, '/');
    const char *b = strrchr(pad, '\\');
    const char *laatste = a > b ? a : b;
    return laatste ? laatste + 1 : pad;
}

static int eindigt_op_exe(const char *naam)
{
    size_t n = strlen(naam);
    return n > 4 && strcasecmp(naam + n - 4, ".exe") == 0;
}

/* Alles wat we voor het indelen van één proces nodig hebben, lui ingelezen */
struct info {
    int pid;
    const char *naam;
    char cmdline[4096]; /* stukken met spaties ertussen */
    char exe[64];       /* bestandsnaam van argv[0], kleine letters */
    char omgeving[32768];
    int omgeving_lengte; /* -1 = nog niet gelezen of niet leesbaar */
    int omgeving_gelezen;
};

static void lees_omgeving(struct info *in, const struct config *c)
{
    if (in->omgeving_gelezen) {
        return;
    }
    in->omgeving_gelezen = 1;
    in->omgeving_lengte = -1;
    if (!c->lees_omgeving) {
        return;
    }
    char pad[512];
    if (pad_maak(pad, sizeof(pad), "/proc/%d/environ", in->pid) == 0) {
        in->omgeving_lengte = lees_bestand(pad, in->omgeving, sizeof(in->omgeving));
    }
}

static long steam_appid(struct info *in, const struct config *c)
{
    lees_omgeving(in, c);
    if (in->omgeving_lengte <= 0) {
        return 0;
    }
    const char *w = zoek_omgeving(in->omgeving, in->omgeving_lengte, "SteamAppId");
    return w ? strtol(w, NULL, 10) : 0;
}

static int regel_past(const struct herken *h, struct info *in, const struct config *c)
{
    switch (h->soort) {
    case HERKEN_APPID:
        return steam_appid(in, c) == strtol(h->waarde, NULL, 10);
    case HERKEN_NAAM:
        return naam_past(in->naam, h->waarde);
    case HERKEN_EXE:
        return strcmp(in->exe, h->waarde) == 0;
    case HERKEN_CMDLINE:
        return strcasestr(in->cmdline, h->waarde) != NULL;
    }
    return 0;
}

/* Bepaalt wat voor proces dit is. */
static void deel_in(struct proces *p, const struct config *c)
{
    p->klasse = KLASSE_ANDERS;
    p->bron = BRON_GEEN;
    p->game = -1;
    p->appid = 0;
    p->exe[0] = '\0';

    /* struct info is groot (36 kB), dus niet op de stack maar één keer op de heap */
    static struct info *in = NULL;
    if (in == NULL && (in = malloc(sizeof(*in))) == NULL) {
        return;
    }
    in->pid = p->pid;
    in->naam = p->naam;
    in->omgeving_gelezen = 0;
    in->exe[0] = '\0';

    char pad[512];
    int lengte = -1;
    if (pad_maak(pad, sizeof(pad), "/proc/%d/cmdline", p->pid) == 0) {
        lengte = lees_bestand(pad, in->cmdline, sizeof(in->cmdline));
    }
    if (lengte <= 0) {
        return; /* kernel-thread of al verdwenen: geen game */
    }
    kopieer_tekst(in->exe, sizeof(in->exe), basisnaam(in->cmdline)); /* argv[0] */
    for (char *s = in->exe; *s; s++) {
        *s = (char)tolower((unsigned char)*s);
    }
    for (int i = 0; i < lengte; i++) { /* NUL-tekens tussen argumenten -> spaties */
        if (in->cmdline[i] == '\0') {
            in->cmdline[i] = ' ';
        }
    }

    /* De Steam-client zelf is nooit een game, ook niet als hij toevallig een SteamAppId heeft */
    if (strcmp(p->naam, "steam") == 0 || strcmp(p->naam, "steamwebhelper") == 0) {
        return;
    }

    /* 1. Games uit rafitune.conf gaan voor */
    for (int g = 0; g < c->aantal_games; g++) {
        for (int r = 0; r < c->games[g].aantal_regels; r++) {
            if (regel_past(&c->games[g].regels[r], in, c)) {
                p->klasse = KLASSE_GAME;
                p->bron = BRON_CONFIG;
                p->game = g;
                return;
            }
        }
    }

    /* 2. Elk Steam-spel */
    if (c->herken_steam) {
        long appid = steam_appid(in, c);
        if (appid > 0) {
            p->klasse = KLASSE_GAME;
            p->bron = BRON_STEAM;
            p->appid = appid;
            return;
        }
    }

    /* 3. Een Windows-programma (.exe) dat niet op de negeerlijst staat */
    if (c->herken_exe && eindigt_op_exe(in->exe)) {
        int negeren = 0;
        for (int i = 0; i < c->aantal_negeer && !negeren; i++) {
            negeren = strcmp(in->exe, c->negeer_exe[i]) == 0;
        }
        if (!negeren) {
            p->klasse = KLASSE_GAME;
            p->bron = BRON_EXE;
            kopieer_tekst(p->exe, sizeof(p->exe), in->exe);
            return;
        }
    }

    /* 4. Een achtergrondtaak die we mogen afremmen */
    for (int i = 0; i < c->aantal_afremmen; i++) {
        if (naam_past(p->naam, c->afremmen[i])) {
            p->klasse = KLASSE_AFREMMEN;
            return;
        }
    }
}

static struct proces *zoek(struct proces_lijst *l, int pid)
{
    for (size_t i = 0; i < l->aantal; i++) {
        if (l->p[i].pid == pid) {
            return &l->p[i];
        }
    }
    return NULL;
}

static struct proces *voeg_toe(struct proces_lijst *l)
{
    if (l->aantal == l->ruimte) {
        size_t nieuw = l->ruimte ? l->ruimte * 2 : 512;
        struct proces *groter = realloc(l->p, nieuw * sizeof(*groter));
        if (groter == NULL) {
            return NULL;
        }
        l->p = groter;
        l->ruimte = nieuw;
    }
    return &l->p[l->aantal++];
}

int proces_scan(struct proces_lijst *l, const struct config *c)
{
    char pad[512];
    if (pad_maak(pad, sizeof(pad), "/proc") != 0) {
        return -1;
    }
    DIR *map = opendir(pad);
    if (map == NULL) {
        return -1;
    }

    for (size_t i = 0; i < l->aantal; i++) {
        l->p[i].gezien = 0;
    }

    int ik = pad_test() ? -1 : (int)getpid();
    struct dirent *e;
    while ((e = readdir(map)) != NULL) {
        if (!isdigit((unsigned char)e->d_name[0])) {
            continue; /* alleen mappen met een nummer zijn processen */
        }
        int pid = atoi(e->d_name);
        if (pid <= 0 || pid == ik) {
            continue;
        }
        char naam[16];
        unsigned long long start;
        if (lees_stat(pid, naam, &start) != 0) {
            continue; /* net verdwenen */
        }

        struct proces *p = zoek(l, pid);
        if (p != NULL && p->start == start && strcmp(p->naam, naam) == 0) {
            p->gezien = 1; /* bekend proces: niets opnieuw lezen */
            continue;
        }
        if (p == NULL && (p = voeg_toe(l)) == NULL) {
            log_fout("te weinig geheugen voor de proceslijst");
            break;
        }
        /* Nieuw proces, hergebruikt pid, of nieuwe naam (exec): opnieuw indelen */
        memset(p, 0, sizeof(*p));
        p->pid = pid;
        p->start = start;
        kopieer_tekst(p->naam, sizeof(p->naam), naam);
        p->gezien = 1;
        deel_in(p, c);
    }
    closedir(map);

    /* Verdwenen processen weghalen (laatste in het gat zetten) */
    for (size_t i = 0; i < l->aantal;) {
        if (!l->p[i].gezien) {
            l->p[i] = l->p[--l->aantal];
        } else {
            i++;
        }
    }
    return 0;
}

void proces_lijst_vrij(struct proces_lijst *l)
{
    free(l->p);
    l->p = NULL;
    l->aantal = l->ruimte = 0;
}

void proces_sleutel(const struct proces *p, char *uit, size_t n)
{
    switch (p->bron) {
    case BRON_CONFIG:
        snprintf(uit, n, "cfg:%d", p->game);
        break;
    case BRON_STEAM:
        snprintf(uit, n, "appid:%ld", p->appid);
        break;
    case BRON_EXE:
        snprintf(uit, n, "exe:%s", p->exe);
        break;
    default:
        snprintf(uit, n, "-");
    }
}

void proces_game_naam(const struct proces *p, const struct config *c, char *uit, size_t n)
{
    switch (p->bron) {
    case BRON_CONFIG:
        snprintf(uit, n, "%s", c->games[p->game].naam);
        break;
    case BRON_STEAM:
        snprintf(uit, n, "Steam-game %ld", p->appid);
        break;
    case BRON_EXE:
        snprintf(uit, n, "%s", p->exe);
        break;
    default:
        snprintf(uit, n, "-");
    }
}

void proces_herkend(const struct proces *p, char *uit, size_t n)
{
    switch (p->bron) {
    case BRON_CONFIG:
        snprintf(uit, n, "rafitune.conf");
        break;
    case BRON_STEAM:
        snprintf(uit, n, "Steam (appid %ld)", p->appid);
        break;
    case BRON_EXE:
        snprintf(uit, n, "Windows-programma");
        break;
    default:
        snprintf(uit, n, "-");
    }
}
