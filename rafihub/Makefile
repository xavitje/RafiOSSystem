# RafiHub – Makefile (een dun laagje over CMake, net als bij RafiTune en RafiBench)
# Bouwen:       make
# Testen:       make test
# Starten:      ./bouw/rafihub
# Installeren:  make install PREFIX=/usr   (gebeurt automatisch in het image)

BOUWMAP ?= bouw
PREFIX  ?= /usr/local

all: $(BOUWMAP)/CMakeCache.txt
	cmake --build $(BOUWMAP) -j

$(BOUWMAP)/CMakeCache.txt: CMakeLists.txt
	cmake -S . -B $(BOUWMAP) -DCMAKE_BUILD_TYPE=Release

test: all
	cd $(BOUWMAP) && ctest --output-on-failure

install: all
	DESTDIR=$(DESTDIR) cmake --install $(BOUWMAP) --prefix $(PREFIX)

clean:
	rm -rf $(BOUWMAP)

.PHONY: all test install clean
