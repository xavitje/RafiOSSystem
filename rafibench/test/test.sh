#!/bin/bash
# RafiBench – tests met zelfgemaakte MangoHud-logs.
# Gebruik: test/test.sh ./rafibench-analyse

set -u
PROG="$1"
MAP="$(mktemp -d)"
trap 'rm -rf "$MAP"' EXIT
GOED=0; FOUT=0

check() { # naam, verwacht (tekst), echte uitvoer
    # spaties samenvoegen, zodat uitlijning niet uitmaakt
    if grep -qF -- "$(tr -s ' ' <<<"$2")" <<<"$(tr -s ' ' <<<"$3")"; then
        GOED=$((GOED + 1))
    else
        FOUT=$((FOUT + 1)); echo "FOUT: $1"; echo "  verwacht: $2"; echo "  kreeg:"; sed 's/^/    /' <<<"$3"
    fi
}

# Maakt een log met systeeminfo-kop, zoals MangoHud dat doet.
# Argumenten: bestand, dan paren "aantal frametijd"
maak_log() {
    local uit="$1"; shift
    {
        echo "os,cpu,gpu,ram,kernel,driver,cpuscheduler"
        echo "RafiOS 0.1,Test-CPU,Test-GPU,16 GB,6.17,Mesa,performance"
        echo "fps,frametime,cpu_load,gpu_load,cpu_temp,gpu_temp,elapsed"
        while [ $# -gt 0 ]; do
            awk -v n="$1" -v ms="$2" 'BEGIN { for (i = 0; i < n; i++) printf "%.1f,%.3f,30,90,60,70,%d\n", 1000/ms, ms, i }'
            shift 2
        done
    } > "$uit"
}

# 1. Elk frame 10 ms -> precies 100 fps, geen haperingen
maak_log "$MAP/vlak.csv" 1000 10
UIT="$("$PROG" "$MAP/vlak.csv")"
check "vlak: gemiddeld" "Gemiddelde fps              100.0" "$UIT"
check "vlak: 1% low" "1% low (fps)               100.0" "$UIT"
check "vlak: haperingen" "Haperingen                     0" "$UIT"

# 2. 990 frames 10 ms + 10 frames 50 ms
#    gemiddeld = 1000 frames / 10,4 s = 96,2 fps; traagste 1% = 10 frames van 50 ms = 20 fps
maak_log "$MAP/hapert.csv" 990 10 10 50
UIT="$("$PROG" "$MAP/hapert.csv")"
check "hapert: gemiddeld" "Gemiddelde fps               96.2" "$UIT"
check "hapert: 1% low" "1% low (fps)                20.0" "$UIT"
check "hapert: 0,1% low" "0,1% low (fps)              20.0" "$UIT"
check "hapert: haperingen" "Haperingen                    10" "$UIT"
check "hapert: traagste" "Traagste frame             50.00 ms" "$UIT"

# 3. Vergelijken: vlak (nieuw) tegen hapert (vorige) -> soepeler
UIT="$("$PROG" --vorige "$MAP/hapert.csv" "$MAP/vlak.csv")"
check "vergelijk: 1% low" "+400.0%" "$UIT"
check "vergelijk: conclusie" "soepeler dan de vorige meting" "$UIT"

# 4. Bijna gelijk -> geen duidelijk verschil
maak_log "$MAP/vlak2.csv" 1000 10.1
UIT="$("$PROG" --vorige "$MAP/vlak.csv" "$MAP/vlak2.csv")"
check "ruis" "geen duidelijk verschil" "$UIT"

# 5. Windows-regeleindes (CRLF) moeten ook werken
sed 's/$/\r/' "$MAP/vlak.csv" > "$MAP/crlf.csv"
UIT="$("$PROG" "$MAP/crlf.csv")"
check "crlf" "Gemiddelde fps              100.0" "$UIT"

# 6. TSV-uitvoer voor het script
UIT="$("$PROG" --tsv "$MAP/hapert.csv")"
check "tsv" "$(printf '1000\t10.4\t96.2\t20.0\t20.0')" "$UIT"

# 7. Fouten netjes afhandelen
echo "dit,is,geen,log" > "$MAP/fout.csv"
UIT="$("$PROG" "$MAP/fout.csv" 2>&1)"; CODE=$?
check "geen log: melding" "geen MangoHud-log" "$UIT"
check "geen log: exitcode" "1" "$CODE"
maak_log "$MAP/kort.csv" 5 10
UIT="$("$PROG" "$MAP/kort.csv" 2>&1)"
check "te kort" "te weinig frames" "$UIT"
UIT="$("$PROG" "$MAP/bestaat-niet.csv" 2>&1)"
check "bestaat niet" "kan '" "$UIT"

echo "Tests: $GOED goed, $FOUT fout"
[ "$FOUT" -eq 0 ]
