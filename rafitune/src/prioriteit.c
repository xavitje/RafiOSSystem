/*
 * RafiTune – prioriteit.c
 *
 * nice loopt van -20 (meeste voorrang) tot 19 (minste). Normaal is 0.
 * Een negatieve waarde instellen mag alleen root (of met CAP_SYS_NICE).
 *
 * In testmodus (RAFITUNE_ROOT gezet) gebruiken we bestandjes
 * proc/<pid>/task/<tid>/rafitune_nice en rafitune_io in plaats van echte
 * systeemaanroepen, zodat een test nooit een echt proces aanpast.
 */

#include "prioriteit.h"

#include "algemeen.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>

#define IOPRIO_WHO_PROCESS 1

/* ---- Lezen en schrijven per thread (echt of in testmodus) ---- */

static int test_lees(int pid, int tid, const char *wat, int *uit)
{
    char pad[512];
    char buf[32];
    if (pad_maak(pad, sizeof(pad), "/proc/%d/task/%d/%s", pid, tid, wat) != 0) {
        return -1;
    }
    *uit = (lees_regel(pad, buf, sizeof(buf)) == 0) ? atoi(buf) : 0;
    return 0;
}

static int test_schrijf(int pid, int tid, const char *wat, int waarde)
{
    char pad[512];
    char buf[32];
    if (pad_maak(pad, sizeof(pad), "/proc/%d/task/%d/%s", pid, tid, wat) != 0) {
        return -1;
    }
    snprintf(buf, sizeof(buf), "%d\n", waarde);
    return schrijf_veilig(pad, buf);
}

static int nice_lees(int pid, int tid, int *uit)
{
    if (pad_test()) {
        return test_lees(pid, tid, "rafitune_nice", uit);
    }
    errno = 0;
    int w = getpriority(PRIO_PROCESS, (id_t)tid); /* -1 kan een echte waarde zijn */
    if (w == -1 && errno != 0) {
        return -1;
    }
    *uit = w;
    return 0;
}

static int nice_zet(int pid, int tid, int waarde)
{
    if (proef_modus) {
        return 0;
    }
    if (pad_test()) {
        return test_schrijf(pid, tid, "rafitune_nice", waarde);
    }
    return setpriority(PRIO_PROCESS, (id_t)tid, waarde);
}

static int io_lees(int pid, int tid, int *uit)
{
    if (pad_test()) {
        return test_lees(pid, tid, "rafitune_io", uit);
    }
    long w = syscall(SYS_ioprio_get, IOPRIO_WHO_PROCESS, tid);
    if (w < 0) {
        return -1;
    }
    *uit = (int)w;
    return 0;
}

static int io_zet(int pid, int tid, int waarde)
{
    if (proef_modus) {
        return 0;
    }
    if (pad_test()) {
        return test_schrijf(pid, tid, "rafitune_io", waarde);
    }
    return syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, tid, waarde) < 0 ? -1 : 0;
}

/* ---- Alle threads van een proces langs ---- */

/* Roept 'doe' aan voor elke thread. Geeft het aantal keren dat 'doe' 1 teruggaf. */
static int voor_elke_thread(struct proces *p, int (*doe)(struct proces *, int tid, void *),
                            void *extra)
{
    char pad[512];
    if (pad_maak(pad, sizeof(pad), "/proc/%d/task", p->pid) != 0) {
        return -1;
    }
    DIR *map = opendir(pad);
    if (map == NULL) {
        return -1; /* proces is al weg */
    }
    int aantal = 0;
    struct dirent *e;
    while ((e = readdir(map)) != NULL) {
        if (!isdigit((unsigned char)e->d_name[0])) {
            continue;
        }
        if (doe(p, atoi(e->d_name), extra) == 1) {
            aantal++;
        }
    }
    closedir(map);
    return aantal;
}

/* Richting: -1 = alleen meer voorrang geven (game), +1 = alleen minder (afremmen) */
struct nice_opdracht {
    int doel;
    int richting;
    int fouten;
};

static int thread_nice(struct proces *p, int tid, void *extra)
{
    struct nice_opdracht *o = extra;
    int nu;
    if (nice_lees(p->pid, tid, &nu) != 0) {
        return 0;
    }
    int moet = (o->richting < 0) ? (nu > o->doel) : (nu < o->doel);
    if (!moet) {
        return 0;
    }
    if (nice_zet(p->pid, tid, o->doel) != 0) {
        o->fouten++;
        return 0;
    }
    return 1;
}

static int thread_io_idle(struct proces *p, int tid, void *extra)
{
    (void)extra;
    int nu;
    if (io_lees(p->pid, tid, &nu) != 0 || nu == IOPRIO_IDLE) {
        return 0;
    }
    return io_zet(p->pid, tid, IOPRIO_IDLE) == 0;
}

static int pas_nice_aan(struct proces *p, int doel, int richting)
{
    if (!p->aangepast) {
        /* Eerste keer: de oude waarde van de hoofdthread onthouden */
        if (nice_lees(p->pid, p->pid, &p->oude_nice) != 0) {
            return -1;
        }
        p->doel_nice = doel;
    }
    struct nice_opdracht o = {doel, richting, 0};
    int aantal = voor_elke_thread(p, thread_nice, &o);
    if (o.fouten > 0 && !p->aangepast) {
        log_fout("kan voorrang van %s (pid %d) niet aanpassen: %s", p->naam, p->pid,
                 strerror(errno));
    }
    p->aangepast = 1; /* ook bij 0 threads: volgende controle pakt nieuwe threads mee */
    return aantal;
}

int prioriteit_game(struct proces *p, int doel)
{
    return pas_nice_aan(p, doel, -1);
}

int prioriteit_afremmen(struct proces *p)
{
    int aantal = pas_nice_aan(p, 19, +1);
    if (!p->io_aangepast) {
        if (io_lees(p->pid, p->pid, &p->oude_io) != 0) {
            return aantal;
        }
        p->io_aangepast = 1;
    }
    voor_elke_thread(p, thread_io_idle, NULL);
    return aantal;
}

/* ---- Terugzetten ---- */

static int thread_herstel(struct proces *p, int tid, void *extra)
{
    (void)extra;
    int nu;
    int iets = 0;
    if (p->aangepast && nice_lees(p->pid, tid, &nu) == 0 && nu == p->doel_nice) {
        iets |= nice_zet(p->pid, tid, p->oude_nice) == 0;
    }
    if (p->io_aangepast && io_lees(p->pid, tid, &nu) == 0 && nu == IOPRIO_IDLE) {
        iets |= io_zet(p->pid, tid, p->oude_io) == 0;
    }
    return iets;
}

void prioriteit_herstel(struct proces *p)
{
    if (!p->aangepast && !p->io_aangepast) {
        return;
    }
    voor_elke_thread(p, thread_herstel, NULL);
    p->aangepast = 0;
    p->io_aangepast = 0;
}
