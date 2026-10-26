# RafiOS – Containerfile
# Dit bestand beschrijft het hele besturingssysteem. GitHub Actions bouwt er
# automatisch een image van; je pc downloadt dat image als update.
#
# Er zijn twee varianten (zie .github/workflows/build.yml):
#   rafios         AMD/Intel-videokaart (en virtuele machines)
#   rafios-nvidia  NVIDIA GTX 16xx / RTX en nieuwer
# Ze verschillen alleen in BASE_IMAGE.

# De basis: Bazzite (Universal Blue). Heeft al een gesigneerde kernel,
# drivers, Steam, Gamescope, MangoHud en Proton-tools.
# De Universal Blue-sleutel staat op Epics lijst voor EAC Linux v2 (Fortnite).
ARG BASE_IMAGE=ghcr.io/ublue-os/bazzite:stable

# Stap 1: bouwbestanden klaarzetten (komen zelf niet in het eindimage)
FROM scratch AS ctx
COPY build_files /
COPY system_files /system_files

# Stap 2: eigen programma's bouwen en testen in een aparte "bouwer".
# Compilers en ontwikkelpakketten blijven hier; alleen de klare programma's
# gaan naar RafiOS. Faalt een test, dan stopt de hele build.
FROM ${BASE_IMAGE} AS bouwer
COPY build_files/bouw-programmas.sh /src/
COPY rafibench /src/rafibench
COPY rafitune /src/rafitune
COPY rafihub /src/rafihub
RUN --mount=type=cache,dst=/var/cache \
    --mount=type=tmpfs,dst=/tmp \
    bash /src/bouw-programmas.sh

# Stap 3: het eigenlijke RafiOS
FROM ${BASE_IMAGE}
ARG IMAGE_NAME=rafios
ARG IMAGE_VENDOR=xavitje

COPY --from=bouwer /uit/ /

RUN --mount=type=bind,from=ctx,source=/,target=/ctx \
    --mount=type=cache,dst=/var/cache \
    --mount=type=cache,dst=/var/log \
    --mount=type=tmpfs,dst=/tmp \
    /ctx/build.sh

# Stap 4: controleren of het image klopt
RUN bootc container lint
