# RafiOS – Containerfile
# Dit bestand beschrijft het hele besturingssysteem. GitHub Actions bouwt er
# automatisch een image van; je pc downloadt dat image als update.

# Stap 1: bouwbestanden klaarzetten (komen zelf niet in het eindimage)
FROM scratch AS ctx
COPY build_files /
COPY system_files /system_files

# Stap 2: de basis.
# Bazzite (Universal Blue) heeft al: gesigneerde kernel, NVIDIA/AMD-drivers,
# Steam, Gamescope, GameMode, MangoHud en Proton-tools.
# De Universal Blue-sleutel staat op Epics lijst voor EAC Linux v2 (Fortnite).
#
# Andere opties om te testen in fase 1:
# FROM ghcr.io/ublue-os/bazzite-nvidia:stable   (NVIDIA-kaart)
# FROM ghcr.io/ublue-os/bazzite-gnome:stable    (GNOME in plaats van KDE)
# FROM ghcr.io/ublue-os/base-main:latest        (kale basis, alles zelf toevoegen)
FROM ghcr.io/ublue-os/bazzite:stable

# Stap 3: RafiOS-aanpassingen (zie build_files/build.sh)
RUN --mount=type=bind,from=ctx,source=/,target=/ctx \
    --mount=type=cache,dst=/var/cache \
    --mount=type=cache,dst=/var/log \
    --mount=type=tmpfs,dst=/tmp \
    /ctx/build.sh

# Stap 4: controleren of het image klopt
RUN bootc container lint
