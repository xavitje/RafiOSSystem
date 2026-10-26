#!/bin/bash
# RafiTune – tests met een nep-/proc en nep-/sys.
# Gebruik: test/test.sh ./rafitune
#
# RAFITUNE_ROOT laat RafiTune in een testmap kijken in plaats van in het echte
# systeem. Er wordt dus nooit een echt proces of de echte CPU aangepast.

set -u
PROG="$(realpath "$1")"
R="$(mktemp -d)"
export RAFITUNE_ROOT="$R"
DAEMON_PID=""
trap '[ -n "$DAEMON_PID" ] && kill -9 "$DAEMON_PID" 2>/dev/null; rm -rf "$R"' EXIT

GOED=0
FOUT=0
ok()   { GOED=$((GOED + 1)); }
fout() { FOUT=$((FOUT + 1)); echo "FOUT: $1"; [ -n "${2:-}" ] && sed 's/^/    /' <<<"$2"; }

# check <naam> <verwacht> <werkelijk>
check() { if [ "$2" = "$3" ]; then ok; else fout "$1" "verwacht: '$2'"$'\n'"kreeg:    '$3'"; fi; }
# bevat <naam> <tekst> <uitvoer>
bevat() { if grep -qF -- "$2" <<<"$3"; then ok; else fout "$1: '$2' niet gevonden" "$3"; fi; }
mist()  { if grep -qF -- "$2" <<<"$3"; then fout "$1: '$2' had er niet mogen zijn" "$3"; else ok; fi; }

# Wacht (max 4 s) tot een opdracht slaagt
wacht() { for _ in $(seq 40); do eval "$1" && return 0; sleep 0.1; done; return 1; }

START=1000
# maak_proces <pid> <naam> <cmdline, stukken met |> <environ, stukken met |> [threads...]
maak_proces() {
    local pid="$1" naam="$2" cmd="$3" env="$4"; shift 4
    local d="$R/proc/$pid"
    mkdir -p "$d/task/$pid"
    START=$((START + 1))
    echo "$pid ($naam) S 1 $pid $pid 0 -1 0 0 0 0 0 0 0 0 0 20 0 1 0 $START 0 0" > "$d/stat"
    if [ -n "$cmd" ]; then printf '%s\0' "${cmd//|/$'\n'}" | tr '\n' '\0' > "$d/cmdline"
    else : > "$d/cmdline"; fi
    if [ -n "$env" ]; then printf '%s\0' "${env//|/$'\n'}" | tr '\n' '\0' > "$d/environ"
    else : > "$d/environ"; fi
    for tid in "$@"; do mkdir -p "$d/task/$tid"; done
}
weg() { rm -rf "$R/proc/$1"; }
nice_van() { cat "$R/proc/$1/task/$2/rafitune_nice" 2>/dev/null || echo 0; }
io_van()   { cat "$R/proc/$1/task/$2/rafitune_io" 2>/dev/null || echo 0; }
zet_nice() { echo "$3" > "$R/proc/$1/task/$2/rafitune_nice"; }

CPU="$R/sys/devices/system/cpu/cpufreq"
maak_cpu() {
    for p in policy0 policy1; do
        mkdir -p "$CPU/$p"
        echo "schedutil" > "$CPU/$p/scaling_governor"
        echo "performance schedutil powersave" > "$CPU/$p/scaling_available_governors"
        echo "balance_performance" > "$CPU/$p/energy_performance_preference"
    done
}
gov() { cat "$CPU/$1/scaling_governor"; }
epp() { cat "$CPU/$1/energy_performance_preference"; }
status() { sed -n "s/^$1=//p" "$R/run/rafitune/status" 2>/dev/null; }

CONF="$R/rafitune.conf"
cat > "$CONF" <<'EOF'
[algemeen]
interval = 0.2     # snel, voor de test
prioriteit = -5
afremmen_processen = baloo_file, tracker-miner-fs-3

[game Counter-Strike 2]
herken = appid:730
prioriteit = -10

[game Minecraft]
herken = cmdline:net.minecraft.client
cpu_performance = nee
EOF

start_daemon() { "$PROG" daemon -c "$CONF" "$@" 2>"$R/log" & DAEMON_PID=$!; }
stop_daemon() {
    kill -TERM "$DAEMON_PID" 2>/dev/null
    wait "$DAEMON_PID" 2>/dev/null
    DAEMON_PID=""
}

echo "== Instellingen"
UIT="$("$PROG" test-config -c "$CONF" 2>&1)"; CODE=$?
check "goede config: exitcode" 0 "$CODE"
bevat "goede config: game" "Counter-Strike 2" "$UIT"
bevat "goede config: profiel" "prioriteit -10, cpu performance, afremmen ja" "$UIT"
bevat "goede config: minecraft" "prioriteit -5, cpu ongewijzigd" "$UIT"

cat > "$R/fout.conf" <<'EOF'
interval = 3
[algemeen]
prioriteit = -50
onzin = 1
interval = 0
[game Leeg]
prioriteit = -3
[game Fout]
herken = appid:abc
herken = raar:x
[iets]
EOF
UIT="$("$PROG" test-config -c "$R/fout.conf" 2>&1)"; CODE=$?
check "foute config: exitcode" 1 "$CODE"
bevat "fout: buiten sectie" "fout.conf:1: 'interval' staat niet in een sectie" "$UIT"
bevat "fout: prioriteit" "fout.conf:3: ongeldige waarde '-50' voor prioriteit" "$UIT"
bevat "fout: onbekend" "fout.conf:4: onbekende sleutel 'onzin'" "$UIT"
bevat "fout: interval" "fout.conf:5: interval moet tussen" "$UIT"
bevat "fout: appid" "fout.conf:9: herken moet zijn" "$UIT"
bevat "fout: soort" "fout.conf:10: herken moet zijn" "$UIT"
bevat "fout: sectie" "fout.conf:11: onbekende sectie [iets]" "$UIT"
bevat "fout: geen herken" "[game Leeg] heeft geen 'herken" "$UIT"
UIT="$("$PROG" test-config -c "$R/bestaat-niet.conf" 2>&1)"; CODE=$?
check "ontbrekende -c: exitcode" 1 "$CODE"

echo "== Herkennen (rafitune scan)"
maak_proces 1 systemd "/usr/lib/systemd/systemd" ""
maak_proces 2 kthreadd "" ""                                       # kernel-thread
maak_proces 100 steam "/home/r/.steam/steam" "HOME=/home/r"         # Steam-client: geen game
maak_proces 101 steamwebhelper "/x/steamwebhelper" "SteamAppId=730" # moet genegeerd worden
maak_proces 200 cs2.exe 'Z:\home\r\cs2\game\bin\win64\cs2.exe|-steam' "HOME=/home/r|SteamAppId=730" 201 202 203
maak_proces 210 wineserver "/usr/bin/wineserver" "SteamAppId=730"
maak_proces 300 baloo_file "/usr/bin/baloo_file" "" 301
maak_proces 400 services.exe 'C:\windows\system32\services.exe' ""  # Wine-onderdeel
UIT="$("$PROG" scan -c "$CONF" 2>&1)"
bevat "scan: cs2 via config" "cs2.exe          game       Counter-Strike 2 (via rafitune.conf)" "$UIT"
bevat "scan: wineserver hoort bij cs2" "wineserver       game       Counter-Strike 2" "$UIT"
bevat "scan: baloo" "baloo_file       afremmen" "$UIT"
mist "scan: steam-client" "steam            game" "$UIT"
mist "scan: steamwebhelper" "steamwebhelper   game" "$UIT"
mist "scan: services.exe" "services.exe" "$UIT"
mist "scan: kernel-thread" "kthreadd" "$UIT"
bevat "scan: gekozen" "RafiTune zou nu gamen met: Counter-Strike 2 (2 game-processen, 1 om af te remmen)" "$UIT"
weg 200; weg 210
maak_proces 500 witcher3.exe 'Z:\games\Witcher 3\bin\x64\witcher3.exe' ""
maak_proces 600 java "/usr/bin/java|-Xmx4G|net.minecraft.client.main.Main" ""
maak_proces 700 lutris-game "/usr/bin/lutris-game" "SteamAppId=0"
UIT="$("$PROG" scan -c "$CONF" 2>&1)"
bevat "scan: losse exe" "witcher3.exe (via Windows-programma)" "$UIT"
bevat "scan: minecraft via cmdline" "java             game       Minecraft" "$UIT"
mist "scan: appid 0 is geen game" "lutris-game" "$UIT"
bevat "scan: config wint van exe" "zou nu gamen met: Minecraft" "$UIT"
weg 500; weg 600; weg 700
maak_proces 800 hl2_linux "/home/r/game/hl2_linux" "SteamAppId=220"
UIT="$("$PROG" scan -c "$CONF" 2>&1)"
bevat "scan: steam-game zonder config" "hl2_linux        game       Steam-game 220 (via Steam (appid 220))" "$UIT"
weg 800

echo "== Daemon: game start en stopt"
maak_cpu
zet_nice 300 300 0
start_daemon --keer 100000
wacht '[ "$(status actief)" = nee ]' || fout "daemon schrijft geen status"
check "voor de game: governor" schedutil "$(gov policy0)"

maak_proces 200 cs2.exe 'Z:\cs2\cs2.exe' "SteamAppId=730" 201 202
zet_nice 200 202 -15   # deze thread had de gebruiker al meer voorrang gegeven
wacht '[ "$(status actief)" = ja ]' || fout "game niet herkend door daemon" "$(cat "$R/log")"
wacht '[ "$(nice_van 200 201)" = -10 ]'
check "game: governor" performance "$(gov policy0)"
check "game: governor policy1" performance "$(gov policy1)"
check "game: EPP" performance "$(epp policy0)"
check "game: hoofdthread voorrang" -10 "$(nice_van 200 200)"
check "game: thread voorrang" -10 "$(nice_van 200 201)"
check "game: -15 blijft -15" -15 "$(nice_van 200 202)"
check "afremmen: nice" 19 "$(nice_van 300 300)"
check "afremmen: thread" 19 "$(nice_van 300 301)"
check "afremmen: io idle" 24576 "$(io_van 300 300)"
check "status: game" "Counter-Strike 2" "$(status game)"
check "status: profiel" "prioriteit -10, cpu performance, afremmen ja" "$(status profiel)"
check "status: afgeremd" 1 "$(status afgeremd)"
[ -f "$R/run/rafitune/cpu-origineel" ] && ok || fout "geen crash-bestand met oude CPU-waarden"

mkdir -p "$R/proc/200/task/204"   # game maakt een nieuwe thread
wacht '[ "$(nice_van 200 204)" = -10 ]' && ok || fout "nieuwe thread krijgt geen voorrang"

weg 200
wacht '[ "$(status actief)" = nee ]' || fout "game-einde niet gezien"
check "na de game: governor" schedutil "$(gov policy0)"
check "na de game: EPP" balance_performance "$(epp policy0)"
check "na de game: baloo nice" 0 "$(nice_van 300 300)"
check "na de game: baloo thread" 0 "$(nice_van 300 301)"
check "na de game: baloo io" 0 "$(io_van 300 300)"
[ -f "$R/run/rafitune/cpu-origineel" ] && fout "crash-bestand niet opgeruimd" || ok
LOG="$(cat "$R/log")"
bevat "log: gestart" "game gestart: Counter-Strike 2 (herkend via rafitune.conf)" "$LOG"
bevat "log: afgeremd" "afgeremd: baloo_file" "$LOG"
bevat "log: gestopt" "game gestopt: Counter-Strike 2" "$LOG"

echo "== Daemon: GameMode verandert de CPU tijdens de game"
maak_proces 200 cs2.exe 'Z:\cs2\cs2.exe' "SteamAppId=730"
wacht '[ "$(gov policy0)" = performance ]' || fout "CPU niet op performance"
echo powersave > "$CPU/policy0/scaling_governor"   # iets anders zet hem tijdens de game om
weg 200
wacht '[ "$(status actief)" = nee ]'
check "GameMode: niet overschreven" powersave "$(gov policy0)"
check "GameMode: andere policy wel terug" schedutil "$(gov policy1)"
bevat "GameMode: gemeld" "iets anders (bijv. GameMode)" "$(cat "$R/log")"
echo schedutil > "$CPU/policy0/scaling_governor"

echo "== Daemon: andere game zonder CPU-aanpassing"
maak_proces 600 java "/usr/bin/java|net.minecraft.client.main.Main" ""
wacht '[ "$(status game)" = Minecraft ]' || fout "Minecraft niet herkend"
check "Minecraft: CPU ongemoeid" schedutil "$(gov policy0)"
check "Minecraft: standaard voorrang" -5 "$(nice_van 600 600)"
weg 600
wacht '[ "$(status actief)" = nee ]'

echo "== Daemon: wisselen van game in één controle"
maak_proces 800 hl2_linux "/g/hl2_linux" "SteamAppId=220"
wacht '[ "$(status game)" = "Steam-game 220" ]' || fout "Steam-game 220 niet herkend"
weg 800; maak_proces 500 witcher3.exe 'Z:\w3\witcher3.exe' ""
wacht '[ "$(status game)" = witcher3.exe ]' || fout "wissel naar witcher3 niet gezien"
bevat "wissel: oude gestopt" "game gestopt: Steam-game 220" "$(cat "$R/log")"
weg 500
wacht '[ "$(status actief)" = nee ]'

echo "== Daemon: netjes stoppen (systemctl stop) tijdens een game"
maak_proces 200 cs2.exe 'Z:\cs2\cs2.exe' "SteamAppId=730"
wacht '[ "$(gov policy0)" = performance ]'
wacht '[ "$(nice_van 300 300)" = 19 ]'
stop_daemon
check "SIGTERM: governor terug" schedutil "$(gov policy0)"
check "SIGTERM: baloo terug" 0 "$(nice_van 300 300)"
check "SIGTERM: status" nee "$(status actief)"

echo "== Daemon: crash tijdens een game"
start_daemon --keer 100000
wacht '[ "$(gov policy0)" = performance ]' || fout "CPU niet op performance"
kill -9 "$DAEMON_PID"; wait "$DAEMON_PID" 2>/dev/null; DAEMON_PID=""
check "crash: CPU blijft hangen" performance "$(gov policy0)"
weg 200
"$PROG" daemon -c "$CONF" --keer 1 2>"$R/log"
check "na crash: governor terug" schedutil "$(gov policy0)"
check "na crash: EPP terug" balance_performance "$(epp policy0)"
bevat "na crash: gemeld" "vorige keer niet netjes gestopt" "$(cat "$R/log")"
[ -f "$R/run/rafitune/cpu-origineel" ] && fout "crash-bestand niet opgeruimd" || ok

echo "== Proefmodus verandert niets"
zet_nice 300 300 0
maak_proces 200 cs2.exe 'Z:\cs2\cs2.exe' "SteamAppId=730"
"$PROG" daemon -c "$CONF" --proef --keer 2 2>"$R/log"
check "proef: governor" schedutil "$(gov policy0)"
check "proef: game-voorrang" 0 "$(nice_van 200 200)"
check "proef: baloo" 0 "$(nice_van 300 300)"
bevat "proef: log" "PROEF: zou 2 van de 2 CPU-policies op performance zetten" "$(cat "$R/log")"
bevat "proef: log afremmen" "PROEF: zou afgeremd: baloo_file" "$(cat "$R/log")"
weg 200

echo "== Geen cpufreq (virtuele machine)"
rm -rf "$R/sys"
maak_proces 200 cs2.exe 'Z:\cs2\cs2.exe' "SteamAppId=730"
"$PROG" daemon -c "$CONF" --keer 1 2>"$R/log"; CODE=$?
check "zonder cpufreq: geen crash" 0 "$CODE"
bevat "zonder cpufreq: melding" "geen cpufreq gevonden" "$(cat "$R/log")"
mkdir -p "$R/sys/devices/system/cpu/cpufreq"
"$PROG" daemon -c "$CONF" --keer 1 2>"$R/log"
bevat "lege cpufreq-map: melding" "geen CPU-policies gevonden" "$(cat "$R/log")"

echo "== Status-opdracht"
UIT="$("$PROG" status)"
bevat "status: tekst" "Game actief      nee" "$UIT"
rm -rf "$R/run"
"$PROG" status >/dev/null; CODE=$?
check "status zonder daemon: exitcode" 3 "$CODE"

echo
echo "Tests: $GOED goed, $FOUT fout"
[ "$FOUT" -eq 0 ]
