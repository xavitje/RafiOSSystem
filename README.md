# RafiOS

Een snel, aanpasbaar game-OS. De Linux-kernel (via Fedora/Universal Blue) regelt de hardware; alles wat je ziet en gebruikt wordt stap voor stap zelfgemaakt.

Status: **fase 1, 2 en 3 voorbereid** (versie 0.3). Nog te testen op echte hardware.

## Wat zit er al in

| Onderdeel | Waar | Wat het doet |
| --- | --- | --- |
| Basis | `Containerfile` | Bazzite (Universal Blue): gesigneerde kernel, GPU-drivers, Steam, Gamescope, GameMode, MangoHud |
| Naam | `build_files/build.sh` | Het systeem heet "RafiOS 0.3" |
| Dubbelklik op .exe/.msi | `system_files/usr/bin/rafi-exe` | Start Windows-programma's met Proton; elk programma krijgt een eigen map in `~/Games/RafiOS` |
| Fortnite-check | `system_files/usr/bin/rafi-check` | Controleert de 8 eisen van EAC Linux v2; verandert niets |
| Minecraft | `system_files/usr/share/rafios/flatpaks.list` | Prism Launcher wordt bij de eerste opstart automatisch geïnstalleerd |
| Fps-overlay | `system_files/usr/share/rafios/MangoHud.conf` | RafiOS-stijl voor MangoHud; aan/uit met rechter-Shift + F12 |
| Benchmark | `rafibench/` + `system_files/usr/bin/rafi-bench` | Meet gemiddelde fps, 1% low en 0,1% low en vergelijkt met je vorige meting |
| RafiTune | `rafitune/` + `/etc/rafitune/rafitune.conf` | Daemon in C: herkent games, zet de CPU op volle snelheid, geeft de game voorrang en remt achtergrondtaken af; zet alles terug na de game |

## Mappen

```
Containerfile          het hele OS in één bestand
build_files/build.sh   aanpassingen tijdens het bouwen
system_files/          bestanden die 1-op-1 in het OS komen
rafibench/             benchmark-analyse in C, met tests (fase 2)
rafitune/              performance-daemon in C, met tests (fase 3)
disk_config/           instellingen voor een installatie-ISO
.github/workflows/     automatisch bouwen op GitHub
docs/                  Engelse uitleg van de Universal Blue-template
```

## Thuis aan de slag

> **Op Windows?** Doe alle opdrachten hieronder in **Git Bash** (komt mee met [Git for Windows](https://git-scm.com/download/win)), in de map van dit project.

### 1. Je GitHub-naam invullen

Vervang `JOUW-GITHUB-NAAM` door je GitHub-gebruikersnaam (in kleine letters):

```bash
cd rafios
sed -i 's/JOUW-GITHUB-NAAM/jouwnaam/' image-template.env disk_config/*.toml
```

### 2. Repository maken en uploaden

Maak op GitHub een **openbare** repository `rafios` (leeg, zonder README). De map heeft al een git-geschiedenis, dus je hoeft alleen te uploaden:

```bash
git add -A
git commit -m "Mijn GitHub-naam ingevuld"
git remote add origin https://github.com/jouwnaam/rafios.git
git push -u origin main
```

> De commits staan op naam van "Rafi" met je Gmail-adres. Liever het privé-adres van GitHub? Zet dat met `git config user.email` en doe `git rebase -r --root --exec "git commit --amend --no-edit --reset-author"` vóór het uploaden.

### 3. Signeersleutel maken

GitHub Actions bouwt alleen als het image gesigneerd kan worden. Installeer [cosign](https://docs.sigstore.dev/cosign/system_config/installation/) en de GitHub CLI (`gh`), en doe in de map `rafios`:

```bash
COSIGN_PASSWORD="" cosign generate-key-pair
gh secret set SIGNING_SECRET < cosign.key
git add cosign.pub
git commit -m "Publieke signeersleutel"
git push
```

> **Let op:** `cosign.key` is geheim en mag nooit naar GitHub. Hij staat al in `.gitignore`. Bewaar hem ergens veilig.

### 4. Wachten op de build

Kijk op GitHub bij het tabblad **Actions**. Een groen vinkje betekent dat het image klaar staat op `ghcr.io/jouwnaam/rafios`.

### 5. Testen in een VM

1. Installeer Bazzite in een VM (GNOME Boxes of virt-manager), zoals in het projectplan.
2. Open in de VM een terminal en wissel naar RafiOS:
   ```bash
   sudo bootc switch ghcr.io/jouwnaam/rafios:latest
   systemctl reboot
   ```
3. Na de herstart controleren:
   ```bash
   fastfetch          # moet "RafiOS 0.3" laten zien
   rafi-check         # Fortnite-eisen (in een VM zijn veel "NEE" normaal)
   ```
4. Download een klein Windows-programma (bijvoorbeeld een installer) en dubbelklik erop.

Iets stuk? Bij het opstarten kun je de vorige versie kiezen, of: `sudo bootc rollback`.

### 6. Fase 2 testen

1. **Minecraft:** na de eerste opstart staat Prism Launcher in het menu (even geduld, hij installeert op de achtergrond). Status bekijken: `systemctl status rafios-flatpaks`.
2. **Fps-overlay:** start een game, druk op rechter-Shift + F12.
3. **Benchmark:** zet in Steam bij een game onder *Eigenschappen > Opstartopties*:
   ```
   rafi-bench naam-van-de-game -- %command%
   ```
   Speel 70 seconden (10 wachten + 60 meten) en sluit de game. In een terminal zie je het resultaat met `rafi-bench naam-van-de-game --resultaten`.
4. **Game-modus:** kijk bij het inlogscherm of er een sessie *Steam Gaming Mode* is (komt uit Bazzite).

Elke meting komt in `~/Games/RafiOS/bench/<game>/`. Gebruik `-l` om te noteren wat je veranderd hebt, bijvoorbeeld `rafi-bench cs2 -l rafitune-aan -- %command%`. Zo zie je in fase 3 precies wat RafiTune oplevert.

**Hoe de cijfers werken:**
- *Gemiddelde fps*: alle frames gedeeld door de tijd.
- *1% low*: de gemiddelde fps van de traagste 1% frames. Dit voel je als haperen, dus belangrijker dan het gemiddelde.
- *Verschil onder 3%* is meestal meetruis. Meet dan nog een keer.

### 7. Fase 3 testen: RafiTune

RafiTune start automatisch. Controleren:

```bash
systemctl status rafitune     # draait hij?
rafitune status               # wat doet hij nu?
sudo rafitune scan            # welke games en achtergrondtaken ziet hij?
journalctl -u rafitune -f     # live meekijken (start daarna een game)
```

**De winst meten** (dit is de mijlpaal van fase 3). Meet elke game minstens 2 keer per situatie, zelfde plek in het spel:

```bash
sudo systemctl stop rafitune
# Steam-opstartopties: rafi-bench cs2 -l zonder-rafitune -- %command%
sudo systemctl start rafitune
# Steam-opstartopties: rafi-bench cs2 -l met-rafitune -- %command%
rafi-bench cs2 --resultaten
```

Kijk vooral naar **1% low**. Verwacht geen wonderen: op een snelle pc met een lege achtergrond is de winst klein. Het verschil zie je vooral als er iets op de achtergrond draait (een update, bestanden indexeren) of op een laptop die normaal zuinig draait.

**Instellingen** staan in `/etc/rafitune/rafitune.conf`, met uitleg erin. Na een wijziging: `rafitune test-config` en `sudo systemctl restart rafitune`. Wil je eerst zien wat RafiTune zou doen zonder dat er iets verandert: `sudo systemctl stop rafitune && sudo rafitune daemon --proef`.

**Hoe RafiTune werkt:**

| Als een game start | Na de game |
| --- | --- |
| CPU-governor en EPP op `performance` | Terug naar de oude waarde, maar alleen als niets anders (zoals GameMode) hem intussen veranderd heeft |
| Elke thread van de game voorrang (standaard nice -5) | Game is weg, niets terug te zetten |
| Achtergrondtaken (indexeren, updates) nice 19 en schijf op *idle* | Terug naar hun oude voorrang |

Crasht RafiTune midden in een game, dan zet de volgende start de CPU terug (de oude waarden staan in `/run/rafitune/cpu-origineel`).

### 8. Zelf bouwen en testen

```bash
cd rafitune && make test     # 75 tests met een nep-/proc en nep-/sys
cd ../rafibench && make test # 17 tests
```

Deze tests draaien ook automatisch bij elke image-build: faalt er één, dan komt er geen nieuwe RafiOS-versie.

## Volgende stappen

Fase 1:

- [ ] Lijst maken van wat er standaard draait (`systemctl list-unit-files --state=enabled`) en onnodige services uitzetten in `build.sh`
- [ ] Eigen logo en opstartscherm
- [ ] Kiezen: blijft Bazzite de basis, of `bazzite-nvidia` / `base-main`?
- [ ] Testen op een echte (tweede) schijf

Fase 2:

- [ ] Prism Launcher installeert vanzelf en Minecraft start
- [ ] Een Steam-game en een losse `.exe` draaien goed
- [ ] Eerste benchmark van 2–3 vaste games: dit is je nulmeting voor fase 3

Fase 3:

- [ ] `rafitune status` laat de game zien zodra je speelt
- [ ] Meting met en zonder RafiTune voor 2–3 games (zie stap 7)
- [ ] Profielen in `rafitune.conf` toevoegen voor je eigen games
