# RafiOS

Een snel, aanpasbaar game-OS. De Linux-kernel (via Fedora/Universal Blue) regelt de hardware; alles wat je ziet en gebruikt wordt stap voor stap zelfgemaakt.

Status: **fase 1 – eerste RafiOS-image** (versie 0.1).

## Wat zit er al in

| Onderdeel | Waar | Wat het doet |
| --- | --- | --- |
| Basis | `Containerfile` | Bazzite (Universal Blue): gesigneerde kernel, GPU-drivers, Steam, Gamescope, GameMode, MangoHud |
| Naam | `build_files/build.sh` | Het systeem heet "RafiOS 0.1" |
| Dubbelklik op .exe/.msi | `system_files/usr/bin/rafi-exe` | Start Windows-programma's met Proton; elk programma krijgt een eigen map in `~/Games/RafiOS` |
| Fortnite-check | `system_files/usr/bin/rafi-check` | Controleert de 8 eisen van EAC Linux v2; verandert niets |
| RafiTune (begin) | `rafitune/` | C-programma dat het CPU-gebruik uit `/proc/stat` leest |

## Mappen

```
Containerfile          het hele OS in één bestand
build_files/build.sh   aanpassingen tijdens het bouwen
system_files/          bestanden die 1-op-1 in het OS komen
rafitune/              performance-daemon in C (fase 3)
disk_config/           instellingen voor een installatie-ISO
.github/workflows/     automatisch bouwen op GitHub
docs/                  Engelse uitleg van de Universal Blue-template
```

## Thuis aan de slag

### 1. Je GitHub-naam invullen

Vervang `JOUW-GITHUB-NAAM` door je GitHub-gebruikersnaam (in kleine letters):

```bash
cd rafios
sed -i 's/JOUW-GITHUB-NAAM/jouwnaam/' image-template.env disk_config/*.toml
```

### 2. Repository maken en uploaden

Maak op GitHub een **openbare** repository `rafios` (leeg, zonder README). Dan:

```bash
git init -b main
git add .
git commit -m "RafiOS 0.1: eerste image"
git remote add origin https://github.com/jouwnaam/rafios.git
git push -u origin main
```

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
   fastfetch          # moet "RafiOS 0.1" laten zien
   rafi-check         # Fortnite-eisen (in een VM zijn veel "NEE" normaal)
   ```
4. Download een klein Windows-programma (bijvoorbeeld een installer) en dubbelklik erop.

Iets stuk? Bij het opstarten kun je de vorige versie kiezen, of: `sudo bootc rollback`.

### 6. RafiTune bouwen

```bash
cd rafitune
make
./rafitune -n 5 -k
```

## Volgende stappen (fase 1)

- [ ] Lijst maken van wat er standaard draait (`systemctl list-unit-files --state=enabled`) en onnodige services uitzetten in `build.sh`
- [ ] Eigen logo en opstartscherm
- [ ] Kiezen: blijft Bazzite de basis, of `bazzite-nvidia` / `base-main`?
- [ ] Testen op een echte (tweede) schijf
