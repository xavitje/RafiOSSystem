/*
 * RafiTune – algemeen.h
 * Hulpfuncties die overal gebruikt worden: paden, bestanden lezen/schrijven, loggen.
 *
 * Testen zonder echte hardware:
 * Als de omgevingsvariabele RAFITUNE_ROOT gezet is, zet RafiTune die map voor
 * elk pad (dus /proc wordt $RAFITUNE_ROOT/proc). Zo kunnen de tests een nep-/proc
 * en nep-/sys maken. Prioriteiten worden dan in bestandjes bijgehouden in plaats
 * van echt aangepast, zodat een test nooit een echt proces raakt.
 */

#ifndef RAFITUNE_ALGEMEEN_H
#define RAFITUNE_ALGEMEEN_H

#include <stddef.h>

/* 1 = alleen laten zien wat RafiTune zou doen, niets echt veranderen */
extern int proef_modus;

void pad_init(void);
int pad_test(void); /* 1 als RAFITUNE_ROOT gezet is */

/* Maakt een pad met de testmap ervoor. Geeft -1 als het pad te lang is. */
int pad_maak(char *uit, size_t n, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

/* Leest een heel bestand (ook met NUL-tekens). Geeft het aantal bytes of -1. */
int lees_bestand(const char *pad, char *buf, size_t n);

/* Leest de eerste regel van een bestand, zonder regeleinde. 0 = gelukt. */
int lees_regel(const char *pad, char *buf, size_t n);

/* Schrijft tekst naar een bestaand bestand (zoals sysfs). 0 = gelukt. */
int schrijf_bestand(const char *pad, const char *tekst);

/* Schrijft een bestand veilig: eerst naar een tijdelijk bestand, dan hernoemen. */
int schrijf_veilig(const char *pad, const char *tekst);

/* Maakt een map en alle mappen erboven (zoals mkdir -p). 0 = gelukt of bestond al. */
int maak_map(const char *pad);

/* Kopieert tekst; te lang wordt netjes afgekapt (altijd met een NUL-teken erachter). */
void kopieer_tekst(char *doel, size_t n, const char *bron);

void log_info(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void log_fout(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
