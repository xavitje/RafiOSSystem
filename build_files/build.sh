#!/bin/bash
# RafiOS – bouwscript
# Draait één keer tijdens het bouwen van het image, als root.

set -ouex pipefail

### 1. Eigen bestanden kopiëren
# Alles in system_files/ komt op dezelfde plek in het OS terecht.
# Voorbeeld: system_files/usr/bin/rafi-check -> /usr/bin/rafi-check
cp -avf "/ctx/system_files"/. /

### 2. Naam: RafiOS
# We passen alleen de zichtbare naam aan. ID en ID_LIKE blijven staan,
# want updates en hulpscripts van de basis kijken daarnaar.
RAFIOS_VERSION="0.3"
sed -i \
    -e 's/^NAME=.*/NAME="RafiOS"/' \
    -e "s/^PRETTY_NAME=.*/PRETTY_NAME=\"RafiOS ${RAFIOS_VERSION}\"/" \
    /usr/lib/os-release
grep -q '^VARIANT=' /usr/lib/os-release \
    && sed -i 's/^VARIANT=.*/VARIANT="Gaming"/' /usr/lib/os-release \
    || echo 'VARIANT="Gaming"' >> /usr/lib/os-release
grep -q '^RAFIOS_VERSION=' /usr/lib/os-release \
    || echo "RAFIOS_VERSION=\"${RAFIOS_VERSION}\"" >> /usr/lib/os-release

### 3. Rechten voor RafiOS-scripts
chmod 0755 /usr/bin/rafi-exe /usr/bin/rafi-check /usr/bin/rafi-bench \
    /usr/libexec/rafios-flatpaks

### 4. Dubbelklik op .exe en .msi -> rafi-exe
# Voeg onze koppelingen toe aan de bestaande lijst van de basis (niet overschrijven).
MIME_LIJST=/etc/xdg/mimeapps.list
mkdir -p /etc/xdg
if [ ! -f "$MIME_LIJST" ] || ! grep -q '^\[Default Applications\]' "$MIME_LIJST"; then
    printf '[Default Applications]\n' >> "$MIME_LIJST"
fi
sed -i '/^\[Default Applications\]/r /usr/share/rafios/mimeapps-rafios.list' "$MIME_LIJST"
update-desktop-database /usr/share/applications || true

### 5. Gaming-basis (fase 2)
# Minecraft via Prism Launcher: wordt bij de eerste opstart van Flathub
# geïnstalleerd (lijst: /usr/share/rafios/flatpaks.list).
systemctl enable rafios-flatpaks.service

# MangoHud: RafiOS-overlay, maar alleen als de basis zelf geen instelling heeft
if [ ! -f /etc/MangoHud.conf ]; then
    cp /usr/share/rafios/MangoHud.conf /etc/MangoHud.conf
fi

# Controle: MangoHud en Gamescope moeten in de basis zitten
for PROG in mangohud gamescope; do
    command -v "$PROG" || echo "WAARSCHUWING: $PROG ontbreekt in de basis"
done

### 6. Eigen programma's in C: rafibench (fase 2) en rafitune (fase 3)
# Bouwen, testen en installeren. Faalt een test, dan faalt de hele build:
# zo komt er nooit een kapotte versie in RafiOS.
# gcc wordt daarna weer verwijderd, zodat het image klein blijft.
GCC_WAS_ER=1
rpm -q gcc >/dev/null 2>&1 || GCC_WAS_ER=0
[ "$GCC_WAS_ER" -eq 1 ] || dnf5 install -y gcc make
for PROGRAMMA in rafibench rafitune; do
    cp -r "/ctx/$PROGRAMMA" "/tmp/$PROGRAMMA" # /ctx is alleen-lezen, dus bouwen in /tmp
    make -C "/tmp/$PROGRAMMA" clean
    make -C "/tmp/$PROGRAMMA"
    make -C "/tmp/$PROGRAMMA" test
    make -C "/tmp/$PROGRAMMA" PREFIX=/usr install
done
[ "$GCC_WAS_ER" -eq 1 ] || dnf5 remove -y gcc

# RafiTune: standaard aan. Instellingen in /etc/rafitune/rafitune.conf
systemctl enable rafitune.service

### 7. Extra pakketten
# Komen uit de Fedora- of RPM Fusion-repo's. Voeg hier toe wat je mist.
dnf5 install -y \
    htop \
    fastfetch

### 8. Onnodige software eruit (fase 1, punt 3)
# Eerst in de VM uitzoeken wat er draait:
#   systemctl list-unit-files --state=enabled
#   systemctl --user list-unit-files --state=enabled
# Zet daarna hier wat weg mag. Voorbeelden, pas aan na testen:
# systemctl disable cups.service          # printen
# systemctl disable ModemManager.service  # 4G-modems
# dnf5 remove -y <pakket>

### 9. Services aanzetten
systemctl enable podman.socket
