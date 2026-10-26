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
RAFIOS_VERSION="0.4"
IMAGE_NAME="${IMAGE_NAME:-rafios}"
IMAGE_VENDOR="${IMAGE_VENDOR:-xavitje}"
sed -i \
    -e 's/^NAME=.*/NAME="RafiOS"/' \
    -e "s/^PRETTY_NAME=.*/PRETTY_NAME=\"RafiOS ${RAFIOS_VERSION}\"/" \
    -e 's/^DEFAULT_HOSTNAME=.*/DEFAULT_HOSTNAME="rafios"/' \
    -e 's/^LOGO=.*/LOGO=rafios-logo/' \
    -e "s|^HOME_URL=.*|HOME_URL=\"https://github.com/${IMAGE_VENDOR}/RafiOSSystem\"|" \
    -e "s|^BUG_REPORT_URL=.*|BUG_REPORT_URL=\"https://github.com/${IMAGE_VENDOR}/RafiOSSystem/issues\"|" \
    /usr/lib/os-release
grep -q '^VARIANT=' /usr/lib/os-release \
    && sed -i 's/^VARIANT=.*/VARIANT="Gaming"/' /usr/lib/os-release \
    || echo 'VARIANT="Gaming"' >> /usr/lib/os-release
sed -i '/^RAFIOS_VERSION=/d' /usr/lib/os-release
echo "RAFIOS_VERSION=\"${RAFIOS_VERSION}\"" >> /usr/lib/os-release

### 3. Rechten voor RafiOS-scripts
chmod 0755 /usr/bin/rafi-exe /usr/bin/rafi-check /usr/bin/rafi-bench \
    /usr/libexec/rafios-flatpaks /usr/libexec/rafios-user-setup

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

### 6. Eigen programma's: RafiBench, RafiTune, RafiHub
# Die zijn al gebouwd en getest in de "bouwer"-stap van de Containerfile en
# staan nu in /usr/bin. Hier alleen nog aanzetten.
for PROG in rafibench-analyse rafitune rafihub; do
    test -x "/usr/bin/$PROG" || { echo "FOUT: /usr/bin/$PROG ontbreekt"; exit 1; }
    if ldd "/usr/bin/$PROG" | grep "not found"; then
        echo "FOUT: $PROG mist bibliotheken in RafiOS"
        exit 1
    fi
done
# RafiHub één keer echt starten (zonder scherm) in het eindimage: zo weten we
# zeker dat alle Qt-onderdelen die de interface nodig heeft aanwezig zijn.
HOME=/tmp XDG_RUNTIME_DIR=/tmp QT_QPA_PLATFORM=offscreen /usr/bin/rafihub --zelftest
# RafiTune: standaard aan. Instellingen in /etc/rafitune/rafitune.conf
systemctl enable rafitune.service

### 6b. Branding: alles wat nog "Bazzite" zei
# Welkomstbericht in de terminal en de tips daarin
if [ -d /usr/share/ublue-os/motd ]; then
    cp /usr/share/rafios/motd/template.md /usr/share/ublue-os/motd/template.md
    rm -rf /usr/share/ublue-os/motd/tips
    cp -r /usr/share/rafios/motd/tips /usr/share/ublue-os/motd/tips
fi
# Logo in fastfetch
if [ -f /usr/share/ublue-os/bazzite/logo.txt ]; then
    cp /usr/share/rafios/logo.txt /usr/share/ublue-os/bazzite/logo.txt
fi
# Image-info: fastfetch en het welkomstbericht tonen hiermee de naam van het image.
# "base-image-name" en de rest blijven staan, Bazzite-scripts gebruiken die.
INFO=/usr/share/ublue-os/image-info.json
if [ -f "$INFO" ]; then
    jq --arg naam "$IMAGE_NAME" --arg maker "$IMAGE_VENDOR" \
        '."image-name" = $naam
         | ."image-vendor" = $maker
         | ."image-ref" = ("ostree-image-signed:docker://ghcr.io/" + $maker + "/" + $naam)' \
        "$INFO" > /tmp/image-info.json
    mv /tmp/image-info.json "$INFO"
    cat "$INFO"
fi
# Steam niet automatisch starten: niet voor nieuwe gebruikers (skel), en voor
# bestaande gebruikers één keer via rafios-user-setup.service
rm -f /etc/skel/.config/autostart/steam.desktop /etc/xdg/autostart/steam.desktop
systemctl --global enable rafios-user-setup.service
gtk-update-icon-cache -f /usr/share/icons/hicolor 2>/dev/null || true

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
