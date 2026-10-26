/*
 * RafiTune – daemon.h
 */

#ifndef RAFITUNE_DAEMON_H
#define RAFITUNE_DAEMON_H

#include "config.h"

/* Draait tot SIGTERM/Ctrl+C, of 'max_keer' controles (-1 = oneindig). */
int daemon_draai(const struct config *c, long max_keer);

/* Eén controle en laat zien wat RafiTune ziet (verandert niets). */
int daemon_scan_toon(const struct config *c);

/* Laat /run/rafitune/status zien. */
int daemon_status_toon(void);

#endif
