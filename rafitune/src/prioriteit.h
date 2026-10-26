/*
 * RafiTune – prioriteit.h
 * De voorrang (nice) en schijf-voorrang (ioprio) van processen aanpassen.
 *
 * Belangrijk: op Linux geldt nice per THREAD, niet per proces. Een game heeft
 * tientallen threads, dus we passen elke thread in /proc/<pid>/task/ aan.
 */

#ifndef RAFITUNE_PRIORITEIT_H
#define RAFITUNE_PRIORITEIT_H

#include "proces.h"

/* Laagste schijf-voorrang ("idle": alleen als niemand anders de schijf gebruikt) */
#define IOPRIO_IDLE (3 << 13)

/* Game: zet elke thread op 'doel' (alleen omhoog in voorrang, nooit omlaag).
 * Geeft het aantal aangepaste threads, of -1 bij een fout. */
int prioriteit_game(struct proces *p, int doel);

/* Achtergrondtaak: nice 19 en schijf-voorrang idle. */
int prioriteit_afremmen(struct proces *p);

/* Zet terug wat wij veranderd hebben (alleen threads die nog op onze waarde staan). */
void prioriteit_herstel(struct proces *p);

#endif
