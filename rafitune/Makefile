# RafiTune – Makefile
# Bouwen:       make
# Testen:       make test
# Installeren:  make install PREFIX=/usr   (gebeurt automatisch in het image)

CC      ?= gcc
CFLAGS  ?= -O2 -g
CFLAGS  += -std=c11 -D_GNU_SOURCE -Wall -Wextra -Wpedantic -Wshadow
PREFIX  ?= /usr/local

BRONNEN = src/main.c src/algemeen.c src/config.c src/cpu.c src/cpufreq.c \
          src/daemon.c src/prioriteit.c src/proces.c
KOPPEN  = $(wildcard src/*.h)
PROGRAMMA = rafitune

$(PROGRAMMA): $(BRONNEN) $(KOPPEN)
	$(CC) $(CFLAGS) -o $@ $(BRONNEN)

test: $(PROGRAMMA)
	bash test/test.sh ./$(PROGRAMMA)

install: $(PROGRAMMA)
	install -Dm755 $(PROGRAMMA) $(DESTDIR)$(PREFIX)/bin/$(PROGRAMMA)

clean:
	rm -f $(PROGRAMMA)

.PHONY: test install clean
