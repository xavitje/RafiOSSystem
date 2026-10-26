#!/bin/bash
# RafiOS – eigen programma's bouwen en testen.
# Draait in de "bouwer"-stap van de Containerfile. Alles wordt geïnstalleerd
# in /uit; die map wordt daarna in het echte RafiOS-image gekopieerd.

set -ouex pipefail

# Gereedschap om te bouwen. Via "pkgconfig(...)" en "cmake(...)" kiest dnf zelf
# het juiste pakket, ook als Fedora pakketnamen verandert.
dnf5 install -y \
    gcc gcc-c++ make cmake \
    'cmake(Qt6Core)' 'cmake(Qt6Gui)' 'cmake(Qt6Qml)' 'cmake(Qt6Quick)' \
    'cmake(Qt6QuickControls2)' 'cmake(Qt6Test)' \
    'pkgconfig(sdl2)' qt6-qtsvg

# RafiBench en RafiTune (C)
for PROGRAMMA in rafibench rafitune; do
    make -C "/src/$PROGRAMMA" clean
    make -C "/src/$PROGRAMMA"
    make -C "/src/$PROGRAMMA" test
    make -C "/src/$PROGRAMMA" PREFIX=/usr DESTDIR=/uit install
done

# RafiHub (C++/Qt)
make -C /src/rafihub clean
make -C /src/rafihub
make -C /src/rafihub test
make -C /src/rafihub PREFIX=/usr DESTDIR=/uit install

ls -lR /uit
