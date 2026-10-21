# RafiTune – Makefile
# Bouwen:     make
# Uitvoeren:  ./rafitune -n 5 -k
# Opruimen:   make clean

CC      ?= gcc
CFLAGS  ?= -O2 -g
CFLAGS  += -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Wpedantic

BRONNEN = src/main.c src/cpu.c
PROGRAMMA = rafitune

$(PROGRAMMA): $(BRONNEN) src/cpu.h
	$(CC) $(CFLAGS) -o $@ $(BRONNEN)

clean:
	rm -f $(PROGRAMMA)

.PHONY: clean
