/*
 * RafiTune – cpufreq.c
 *
 * In /sys/devices/system/cpu/cpufreq/policyN/ staan bestanden die bepalen hoe
 * snel de CPU mag lopen:
 *   scaling_governor                 de "governor", bijv. powersave of performance
 *   scaling_available_governors      welke governors er zijn
 *   energy_performance_preference    (EPP) bij moderne AMD/Intel: zuinig <-> snel
 *
 * Bij AMD (amd-pstate) en Intel (intel_pstate) in "active"-modus zet de
 * governor "performance" de EPP automatisch ook op performance.
 *
 * Veiligheid: de oude waarden gaan ook naar /run/rafitune/cpu-origineel.
 * Crasht RafiTune midden in een game, dan zet de volgende start ze terug.
 */

#include "cpufreq.h"

#include "algemeen.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int lees_waarde(const char *policy, const char *bestand, char *uit, size_t n)
{
    char pad[512];
    if (pad_maak(pad, sizeof(pad), "/sys/devices/system/cpu/cpufreq/%s/%s", policy, bestand) !=
        0) {
        return -1;
    }
    return lees_regel(pad, uit, n);
}

static int schrijf_waarde(const char *policy, const char *bestand, const char *waarde)
{
    char pad[512];
    if (pad_maak(pad, sizeof(pad), "/sys/devices/system/cpu/cpufreq/%s/%s", policy, bestand) !=
        0) {
        return -1;
    }
    if (schrijf_bestand(pad, waarde) != 0) {
        log_fout("kan %s niet op '%s' zetten voor %s: %s", bestand, waarde, policy,
                 strerror(errno));
        return -1;
    }
    return 0;
}

static int bevat_woord(const char *lijst, const char *woord)
{
    size_t n = strlen(woord);
    for (const char *p = lijst; (p = strstr(p, woord)) != NULL; p += n) {
        int begin_ok = (p == lijst || p[-1] == ' ');
        int eind_ok = (p[n] == '\0' || p[n] == ' ' || p[n] == '\n');
        if (begin_ok && eind_ok) {
            return 1;
        }
    }
    return 0;
}

static void bestand_crash(char *pad, size_t n)
{
    if (pad_maak(pad, n, "/run/rafitune/cpu-origineel") != 0) {
        pad[0] = '\0';
    }
}

/* Bewaart de oude waarden, zodat een crash niet de CPU op performance laat staan */
static void bewaar_voor_crash(const struct cpufreq_staat *s)
{
    char pad[512];
    char map[512];
    bestand_crash(pad, sizeof(pad));
    if (pad[0] == '\0' || pad_maak(map, sizeof(map), "/run/rafitune") != 0) {
        return;
    }
    maak_map(map); /* bestaat meestal al (systemd RuntimeDirectory) */

    char tekst[CPUFREQ_MAX * 128];
    size_t lengte = 0;
    tekst[0] = '\0';
    for (int i = 0; i < s->aantal; i++) {
        const struct cpufreq_policy *p = &s->p[i];
        int k = snprintf(tekst + lengte, sizeof(tekst) - lengte, "%s %s %s %s %d\n", p->naam,
                         p->oud_gov[0] ? p->oud_gov : "-", p->gezet_gov[0] ? p->gezet_gov : "-",
                         p->oud_epp[0] ? p->oud_epp : "-", p->epp_gezet);
        if (k < 0 || (size_t)k >= sizeof(tekst) - lengte) {
            break;
        }
        lengte += (size_t)k;
    }
    if (schrijf_veilig(pad, tekst) != 0) {
        log_fout("kan %s niet schrijven", pad);
    }
}

int cpufreq_performance(struct cpufreq_staat *s)
{
    memset(s, 0, sizeof(*s));
    char pad[512];
    if (pad_maak(pad, sizeof(pad), "/sys/devices/system/cpu/cpufreq") != 0) {
        return 0;
    }
    DIR *map = opendir(pad);
    if (map == NULL) {
        log_info("geen cpufreq gevonden (virtuele machine?), CPU-snelheid blijft zoals hij is");
        return 0;
    }

    int aangepast = 0;
    struct dirent *e;
    while ((e = readdir(map)) != NULL && s->aantal < CPUFREQ_MAX) {
        if (strncmp(e->d_name, "policy", 6) != 0) {
            continue;
        }
        struct cpufreq_policy *p = &s->p[s->aantal];
        memset(p, 0, sizeof(*p));
        kopieer_tekst(p->naam, sizeof(p->naam), e->d_name);

        char beschikbaar[256] = "";
        if (lees_waarde(p->naam, "scaling_governor", p->oud_gov, sizeof(p->oud_gov)) != 0) {
            continue;
        }
        lees_waarde(p->naam, "scaling_available_governors", beschikbaar, sizeof(beschikbaar));
        if (lees_waarde(p->naam, "energy_performance_preference", p->oud_epp,
                        sizeof(p->oud_epp)) != 0) {
            p->oud_epp[0] = '\0';
        }
        s->aantal++;

        int veranderd = 0;
        /* Eerst de governor */
        if (strcmp(p->oud_gov, "performance") != 0 && bevat_woord(beschikbaar, "performance")) {
            if (proef_modus) {
                veranderd = 1;
            } else if (schrijf_waarde(p->naam, "scaling_governor", "performance") == 0) {
                snprintf(p->gezet_gov, sizeof(p->gezet_gov), "performance");
                veranderd = 1;
            }
        }
        /* Dan EPP, als die (nog) niet op performance staat */
        if (p->oud_epp[0] != '\0') {
            char nu[48];
            if (lees_waarde(p->naam, "energy_performance_preference", nu, sizeof(nu)) == 0 &&
                strcmp(nu, "performance") != 0) {
                if (proef_modus) {
                    veranderd = 1;
                } else if (schrijf_waarde(p->naam, "energy_performance_preference",
                                          "performance") == 0) {
                    p->epp_gezet = 1;
                    veranderd = 1;
                }
            }
        }
        aangepast += veranderd;
    }
    closedir(map);

    s->actief = 1;
    if (s->aantal == 0) {
        log_info("geen CPU-policies gevonden (virtuele machine?), CPU-snelheid blijft zoals hij is");
    } else if (proef_modus) {
        log_info("PROEF: zou %d van de %d CPU-policies op performance zetten", aangepast,
                 s->aantal);
    } else if (aangepast > 0) {
        bewaar_voor_crash(s);
    }
    return aangepast;
}

void cpufreq_herstel(struct cpufreq_staat *s)
{
    if (!s->actief) {
        return;
    }
    if (proef_modus) {
        log_info("PROEF: zou de CPU-instellingen terugzetten");
        s->actief = 0;
        return;
    }

    int teruggezet = 0;
    int overgeslagen = 0;
    for (int i = 0; i < s->aantal; i++) {
        struct cpufreq_policy *p = &s->p[i];
        char nu[48];
        int wij_waren_het = 0;

        /* Governor terug, maar alleen als hij nog op onze waarde staat */
        if (p->gezet_gov[0] != '\0' &&
            lees_waarde(p->naam, "scaling_governor", nu, sizeof(nu)) == 0) {
            if (strcmp(nu, p->gezet_gov) == 0) {
                if (schrijf_waarde(p->naam, "scaling_governor", p->oud_gov) == 0) {
                    wij_waren_het = 1;
                }
            } else {
                overgeslagen++;
            }
        }
        /* EPP terug als wij iets veranderd hebben en hij nog op performance staat */
        if ((p->epp_gezet || wij_waren_het) && p->oud_epp[0] != '\0' &&
            strcmp(p->oud_epp, "performance") != 0 &&
            lees_waarde(p->naam, "energy_performance_preference", nu, sizeof(nu)) == 0 &&
            strcmp(nu, "performance") == 0) {
            if (schrijf_waarde(p->naam, "energy_performance_preference", p->oud_epp) == 0) {
                wij_waren_het = 1;
            }
        }
        teruggezet += wij_waren_het;
    }

    if (teruggezet > 0) {
        log_info("CPU teruggezet (%d policies)", teruggezet);
    }
    if (overgeslagen > 0) {
        log_info("%d CPU-policies niet teruggezet: iets anders (bijv. GameMode) had ze al "
                 "veranderd",
                 overgeslagen);
    }
    s->actief = 0;

    char pad[512];
    bestand_crash(pad, sizeof(pad));
    if (pad[0] != '\0') {
        remove(pad);
    }
}

void cpufreq_herstel_na_crash(void)
{
    char pad[512];
    bestand_crash(pad, sizeof(pad));
    FILE *f = pad[0] ? fopen(pad, "r") : NULL;
    if (f == NULL) {
        return;
    }

    static struct cpufreq_staat s; /* groot, dus niet op de stack */
    memset(&s, 0, sizeof(s));
    char regel[256];
    while (fgets(regel, sizeof(regel), f) != NULL && s.aantal < CPUFREQ_MAX) {
        struct cpufreq_policy *p = &s.p[s.aantal];
        memset(p, 0, sizeof(*p));
        if (sscanf(regel, "%31s %31s %31s %47s %d", p->naam, p->oud_gov, p->gezet_gov,
                   p->oud_epp, &p->epp_gezet) != 5 ||
            strchr(p->naam, '/') != NULL || strncmp(p->naam, "policy", 6) != 0) {
            continue; /* kapotte regel overslaan */
        }
        if (!strcmp(p->oud_gov, "-")) {
            p->oud_gov[0] = '\0';
        }
        if (!strcmp(p->gezet_gov, "-")) {
            p->gezet_gov[0] = '\0';
        }
        if (!strcmp(p->oud_epp, "-")) {
            p->oud_epp[0] = '\0';
        }
        s.aantal++;
    }
    fclose(f);

    log_info("vorige keer niet netjes gestopt: CPU-instellingen worden teruggezet");
    s.actief = 1;
    int bewaar = proef_modus;
    proef_modus = 0; /* terugzetten na een crash gebeurt altijd echt */
    cpufreq_herstel(&s);
    proef_modus = bewaar;
}
