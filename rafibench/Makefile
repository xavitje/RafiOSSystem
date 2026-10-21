# RafiBench – Makefile
# Bouwen:       make
# Testen:       make test
# Installeren:  make install PREFIX=/usr   (gebeurt automatisch in het image)

CC      ?= gcc
CFLAGS  ?= -O2 -g
CFLAGS  += -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Wpedantic
PREFIX  ?= /usr/local

BRONNEN = src/main.c src/meting.c
PROGRAMMA = rafibench-analyse

$(PROGRAMMA): $(BRONNEN) src/meting.h
	$(CC) $(CFLAGS) -o $@ $(BRONNEN)

test: $(PROGRAMMA)
	./test/test.sh ./$(PROGRAMMA)

install: $(PROGRAMMA)
	install -Dm755 $(PROGRAMMA) $(DESTDIR)$(PREFIX)/bin/$(PROGRAMMA)

clean:
	rm -f $(PROGRAMMA)

.PHONY: test install clean
