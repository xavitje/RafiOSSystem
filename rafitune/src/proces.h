/*
 * RafiTune – proces.h
 * Alle processen in /proc langslopen en bepalen welke een game zijn.
 */

#ifndef RAFITUNE_PROCES_H
#define RAFITUNE_PROCES_H

#include "config.h"

#include <stddef.h>

enum klasse {
    KLASSE_ANDERS = 0, /* gewoon proces: niets mee doen */
    KLASSE_GAME,       /* hoort bij een game */
    KLASSE_AFREMMEN,   /* achtergrondtaak die afgeremd mag worden */
};

/* Hoe een game herkend is. Lager = betrouwbaarder. */
enum bron {
    BRON_GEEN = 0,
    BRON_CONFIG, /* staat in rafitune.conf */
    BRON_STEAM,  /* Steam-AppID in de omgeving */
    BRON_EXE,    /* een .exe via Wine/Proton */
};

struct proces {
    int pid;
    unsigned long long start; /* starttijd: samen met pid uniek, ook als een pid hergebruikt wordt */
    char naam[16];            /* procesnaam (comm) */

    enum klasse klasse;
    enum bron bron;
    int game;      /* index in config->games bij BRON_CONFIG */
    long appid;    /* bij BRON_STEAM */
    char exe[64];  /* bij BRON_EXE */

    int gezien;    /* intern: bij deze controle nog gevonden */

    /* Wat RafiTune aan dit proces veranderd heeft (om terug te zetten) */
    int aangepast;
    int oude_nice;
    int doel_nice;
    int io_aangepast;
    int oude_io;
};

struct proces_lijst {
    struct proces *p;
    size_t aantal;
    size_t ruimte;
};

/* Kijkt welke processen er nu zijn. Nieuwe processen worden ingedeeld,
 * verdwenen processen uit de lijst gehaald. Geeft -1 als /proc niet leesbaar is. */
int proces_scan(struct proces_lijst *l, const struct config *c);

void proces_lijst_vrij(struct proces_lijst *l);

/* Unieke sleutel van de game waar dit proces bij hoort, bijv. "appid:730" */
void proces_sleutel(const struct proces *p, char *uit, size_t n);

/* Leesbare naam van de game, bijv. "Counter-Strike 2" of "Steam-game 730" */
void proces_game_naam(const struct proces *p, const struct config *c, char *uit, size_t n);

/* Hoe het herkend is, bijv. "appid 730" of "exe witcher3.exe" */
void proces_herkend(const struct proces *p, char *uit, size_t n);

#endif
