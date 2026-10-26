/*
 * RafiTune – config.h
 * Het instellingenbestand (/etc/rafitune/rafitune.conf) inlezen.
 */

#ifndef RAFITUNE_CONFIG_H
#define RAFITUNE_CONFIG_H

#include <stdio.h>

#define MAX_GAMES 64
#define MAX_REGELS 8   /* herken-regels per game */
#define MAX_NAMEN 48   /* lengte van lijsten (afremmen, negeren) */

#define NIET_GEZET 1000 /* waarde voor "niet ingevuld" bij getallen */

enum herken_soort {
    HERKEN_APPID,   /* appid:730          Steam-spel met dit nummer */
    HERKEN_NAAM,    /* naam:cs2           procesnaam (zoals in top) */
    HERKEN_EXE,     /* exe:game.exe       Windows-programma */
    HERKEN_CMDLINE, /* cmdline:minecraft  tekst in de opdrachtregel */
};

struct herken {
    enum herken_soort soort;
    char waarde[128];
};

/* Wat RafiTune doet als een game draait */
struct profiel {
    int prioriteit;      /* nice-waarde voor de game (-20 = hoogst, 0 = normaal) */
    int cpu_performance; /* 1 = CPU op volle snelheid */
    int afremmen;        /* 1 = achtergrondtaken laagste prioriteit geven */
};

struct game {
    char naam[64];
    struct herken regels[MAX_REGELS];
    int aantal_regels;
    struct profiel profiel; /* NIET_GEZET / -1 = neem de standaard over */
};

struct config {
    double interval;   /* seconden tussen twee controles */
    int lees_omgeving; /* mag RafiTune /proc/<pid>/environ lezen (voor Steam-AppID)? */
    int herken_steam;  /* elk Steam-spel automatisch herkennen */
    int herken_exe;    /* elk .exe-programma automatisch herkennen */
    struct profiel standaard;

    char afremmen[MAX_NAMEN][32]; /* procesnamen die afgeremd worden */
    int aantal_afremmen;
    char negeer_exe[MAX_NAMEN][64]; /* .exe-namen die geen game zijn */
    int aantal_negeer;

    struct game games[MAX_GAMES];
    int aantal_games;
};

/* Vult de standaardinstellingen in. */
void config_standaard(struct config *c);

/* Leest een bestand. Geeft het aantal fouten (0 = goed) of -1 als het niet open kan.
 * Foutmeldingen gaan naar 'fouten'. */
int config_lees(const char *pad, struct config *c, FILE *fouten);

/* Het profiel voor een game (g mag NULL zijn = standaard). */
struct profiel config_profiel(const struct config *c, const struct game *g);

/* Laat de instellingen zien. */
void config_toon(const struct config *c, FILE *uit);

/* Hulpje: past procesnaam 'comm' (max 15 tekens, zoals de kernel hem afkapt)
 * bij een naam uit de instellingen? */
int naam_past(const char *comm, const char *gezocht);

#endif
