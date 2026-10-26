/*
 * RafiTune – algemeen.c
 */

#include "algemeen.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int proef_modus = 0;

static char root[512];

void pad_init(void)
{
    const char *r = getenv("RAFITUNE_ROOT");
    if (r != NULL && r[0] != '\0') {
        snprintf(root, sizeof(root), "%s", r);
    } else {
        root[0] = '\0';
    }
}

int pad_test(void)
{
    return root[0] != '\0';
}

int pad_maak(char *uit, size_t n, const char *fmt, ...)
{
    int k = snprintf(uit, n, "%s", root);
    if (k < 0 || (size_t)k >= n) {
        return -1;
    }
    va_list ap;
    va_start(ap, fmt);
    int m = vsnprintf(uit + k, n - (size_t)k, fmt, ap);
    va_end(ap);
    if (m < 0 || (size_t)m >= n - (size_t)k) {
        return -1;
    }
    return 0;
}

int lees_bestand(const char *pad, char *buf, size_t n)
{
    if (n == 0) {
        return -1;
    }
    int fd = open(pad, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return -1;
    }
    size_t totaal = 0;
    while (totaal < n - 1) {
        ssize_t r = read(fd, buf + totaal, n - 1 - totaal);
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            return -1;
        }
        if (r == 0) {
            break;
        }
        totaal += (size_t)r;
    }
    close(fd);
    buf[totaal] = '\0';
    return (int)totaal;
}

int lees_regel(const char *pad, char *buf, size_t n)
{
    if (lees_bestand(pad, buf, n) < 0) {
        return -1;
    }
    buf[strcspn(buf, "\n")] = '\0';
    return 0;
}

int schrijf_bestand(const char *pad, const char *tekst)
{
    int fd = open(pad, O_WRONLY | O_TRUNC | O_CLOEXEC);
    if (fd < 0) {
        return -1;
    }
    size_t lengte = strlen(tekst);
    ssize_t w = write(fd, tekst, lengte);
    int fout = (w < 0 || (size_t)w != lengte);
    int bewaar = errno;
    if (close(fd) != 0) {
        fout = 1;
    }
    errno = bewaar;
    return fout ? -1 : 0;
}

int schrijf_veilig(const char *pad, const char *tekst)
{
    char tijdelijk[1024];
    if (snprintf(tijdelijk, sizeof(tijdelijk), "%s.tmp", pad) >= (int)sizeof(tijdelijk)) {
        return -1;
    }
    FILE *f = fopen(tijdelijk, "w");
    if (f == NULL) {
        return -1;
    }
    int fout = fputs(tekst, f) < 0;
    if (fclose(f) != 0) {
        fout = 1;
    }
    if (fout || rename(tijdelijk, pad) != 0) {
        unlink(tijdelijk);
        return -1;
    }
    return 0;
}

int maak_map(const char *pad)
{
    char deel[1024];
    kopieer_tekst(deel, sizeof(deel), pad);
    for (char *p = deel + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(deel, 0755) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }
    if (mkdir(deel, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

void kopieer_tekst(char *doel, size_t n, const char *bron)
{
    if (n == 0) {
        return;
    }
    size_t lengte = strnlen(bron, n - 1);
    memcpy(doel, bron, lengte);
    doel[lengte] = '\0';
}

void log_info(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fputs("rafitune: ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

void log_fout(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fputs("rafitune: FOUT: ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}
