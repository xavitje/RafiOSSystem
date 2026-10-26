# RafiOS

Een snel, aanpasbaar game-OS. De Linux-kernel en drivers komen van Bazzite (Fedora); alles wat je ziet en gebruikt wordt stap voor stap zelf gemaakt.

**Versie 0.4** · fase 1 t/m 4 klaar · getest in Hyper-V

## Twee versies

Kies de versie die bij je **videokaart** past:

| Versie | Voor | Image |
| --- | --- | --- |
| `rafios` | AMD- of Intel-videokaart, en virtuele machines (Hyper-V) | `ghcr.io/xavitje/rafios:latest` |
| `rafios-nvidia` | NVIDIA GTX 16xx (zoals de GTX 1650), RTX 20xx en nieuwer | `ghcr.io/xavitje/rafios-nvidia:latest` |

Ze zijn verder precies hetzelfde. GitHub bouwt ze allebei bij elke wijziging.

## Wat zit erin

| Onderdeel | Wat het doet |
| --- | --- |
| **RafiHub** | Je eigen game-launcher: Steam-games en losse `.exe`'s op één plek, met hoesjes, zoeken en controller-bediening |
| **RafiTune** | Herkent games en geeft ze voorrang: CPU op volle snelheid, achtergrondtaken afgeremd. Zet alles terug na de game |
| **rafi-bench** | Meet je fps (gemiddeld, 1% low, 0,1% low) en vergelijkt met de vorige keer |
| **rafi-exe** | Dubbelklik op een `.exe` of `.msi` start hem met Proton, elk programma in een eigen map |
| **rafi-check** | Controleert de 8 eisen van de Fortnite-anticheat (EAC Linux v2) |
| **Branding** | RafiOS-naam, logo in fastfetch, eigen welkomstbericht in de terminal, Steam start niet meer vanzelf |
| **Minecraft** | Prism Launcher wordt bij de eerste opstart geïnstalleerd |

## RafiOS installeren

Je installeert eerst Bazzite en stapt daarna met één opdracht over naar RafiOS. Daarna komen alle updates van jouw GitHub.

### 1. Bazzite installeren

1. Ga naar **bazzite.gg** en kies bij het downloaden je videokaart:
   - laptop met GTX 1650: kies **NVIDIA** (*GTX 16xx and newer*);
   - AMD/Intel of Hyper-V: kies **AMD / Intel**.
2. Zet de ISO op een USB-stick met **Rufus** of **balenaEtcher** (op Windows).
3. Start de laptop vanaf de USB-stick (meestal F12, F2 of Esc bij het opstarten) en installeer Bazzite.
4. Bij het eerste opstarten verschijnt een **blauw scherm** (MokManager): kies **Enroll MOK** → **Continue** → **Yes**, typ `universalblue` en kies **Reboot**. Mis je dit, zie dan *Problemen* hieronder.

### 2. Overstappen naar RafiOS

Open **Konsole** in Bazzite en typ (voor de laptop met NVIDIA):

```
sudo bootc switch ghcr.io/xavitje/rafios-nvidia:latest
systemctl reboot
```

Voor AMD/Intel of Hyper-V gebruik je `ghcr.io/xavitje/rafios:latest`.

Na de herstart: typ `fastfetch`. Staat er **RafiOS 0.4** en het R-logo, dan draai je RafiOS.

Je computernaam is nog `bazzite`. Veranderen:

```
sudo hostnamectl set-hostname rafios
```

### Problemen

| Probleem | Oplossing |
| --- | --- |
| `bad shim signature` bij het opstarten | De sleutel is niet ingeschreven. Zet Secure Boot even uit in de BIOS, start op, typ `ujust enroll-secure-boot-key` (wachtwoord `universalblue`), zet Secure Boot weer aan en volg het blauwe scherm. |
| `Temporary failure in name resolution` | Geen internet. In Hyper-V: VM-instellingen → Netwerkadapter → *Default Switch*. |
| Toetsen zitten op de verkeerde plek | Systeeminstellingen → Keyboard → Layouts: zet **English (US)** erin en haal de rest weg. |
| Update maakte iets stuk | Kies bij het opstarten de vorige versie, of typ `sudo bootc rollback`. |

## RafiOS aanpassen

Zo komt een wijziging van jouw pc in RafiOS:

1. Pas bestanden aan in `D:\Rafios`.
2. In **GitHub Desktop**: beschrijving typen → **Commit to main** → **Push origin**.
3. GitHub bouwt beide versies opnieuw (tabblad **Actions**, ongeveer een half uur). Alle tests moeten slagen, anders komt er geen nieuwe versie.
4. Op de pc met RafiOS: `sudo bootc upgrade` en opnieuw opstarten. (RafiOS kijkt ook zelf regelmatig naar updates.)

Belangrijke bestanden:

```
Containerfile               het hele OS: basis, bouwstap, eindimage
build_files/build.sh        aanpassingen aan het systeem (naam, branding, services)
build_files/bouw-programmas.sh   bouwt en test RafiBench, RafiTune en RafiHub
system_files/               bestanden die 1-op-1 in het OS komen (bijv. /etc/rafitune/rafitune.conf)
rafihub/                    game-launcher (C++ en Qt/QML)
rafitune/                   performance-daemon (C)
rafibench/                  fps-analyse (C)
.github/workflows/build.yml bouwt rafios en rafios-nvidia
```

## De programma's gebruiken

### RafiHub

Staat in het menu onder **Games**, of typ `rafihub`.

- **Spelen:** klik dubbel, of Enter, of **A** op je controller.
- **Zoeken:** Ctrl+F of **Y**. **Filteren:** Alles / Steam / Eigen games.
- **Eigen game toevoegen:** *+ Game toevoegen* → kies een `.exe`, `.msi` of Linux-programma. Hoesje mag je zelf kiezen.
- **Benchmark:** *Spelen met benchmark* (of **B** / **X**). Bij Steam-games kopieert RafiHub de opstartoptie voor `rafi-bench`; die plak je in Steam bij *Eigenschappen > Opstartopties*.
- Onderaan zie je of **RafiTune** bezig is.

Je eigen games staan in `~/.config/rafihub/games.json`.

### RafiTune

Draait vanzelf op de achtergrond.

```
rafitune status          # wat doet RafiTune nu?
sudo rafitune scan       # welke games en achtergrondtaken ziet hij?
journalctl -u rafitune -f    # live meekijken
```

Instellingen en profielen per game: `/etc/rafitune/rafitune.conf` (met uitleg erin). Na een wijziging: `rafitune test-config` en `sudo systemctl restart rafitune`.

### rafi-bench: de winst van RafiTune meten

Meet elke game minstens twee keer per situatie, op dezelfde plek in het spel:

```
sudo systemctl stop rafitune
# Steam-opstartopties: rafi-bench cs2 -l zonder-rafitune -- %command%
sudo systemctl start rafitune
# Steam-opstartopties: rafi-bench cs2 -l met-rafitune -- %command%
rafi-bench cs2 --resultaten
```

Kijk vooral naar **1% low**: dat voel je als haperen. Een verschil onder 3% is meestal meetruis.

## Zelf bouwen en testen (Linux)

```
cd rafitune && make test    # 75 tests met een nep-/proc en nep-/sys
cd rafibench && make test   # 17 tests
cd rafihub && make test     # kern + interface (Qt 6 en CMake nodig)
```

Een screenshot van RafiHub zonder scherm: `QT_QPA_PLATFORM=offscreen rafihub --screenshot rafihub.png`.

## Volgende stappen

- [ ] RafiOS 0.4 in de VM: `sudo bootc upgrade`, herstarten, branding en RafiHub bekijken
- [ ] Laptop (GTX 1650): Bazzite NVIDIA installeren en overstappen naar `rafios-nvidia`
- [ ] Op de laptop: games spelen en met `rafi-bench` meten met en zonder RafiTune
- [ ] Fase 5: RafiShell, de eigen desktop
