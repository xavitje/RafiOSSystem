// RafiHub – steambibliotheek.h
// Zoekt welke Steam-games geïnstalleerd zijn, zonder Steam zelf te starten.
//
// Steam zet per bibliotheek (schijf) een map steamapps/ neer met per game een
// bestand appmanifest_<appid>.acf. Welke bibliotheken er zijn staat in
// steamapps/libraryfolders.vdf.

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

struct SteamGame {
    QString appid;
    QString naam;
    QString installPad;
    QString cover; // pad naar een plaatje, of leeg
    qint64 laatstGespeeld = 0; // Unix-tijd, 0 = nooit / onbekend
};

class SteamBibliotheek {
public:
    // Plekken waar Steam kan staan (gewone installatie en Flatpak)
    static QStringList kandidaatMappen(const QString &home);

    // De eerste plek die echt een Steam-map is, of leeg
    static QString vindSteamMap(const QString &home);

    // Alle geïnstalleerde games. Fouten in losse bestanden worden overgeslagen;
    // 'waarschuwingen' (mag nullptr zijn) krijgt een regel per probleem.
    static QList<SteamGame> lees(const QString &steamMap, QStringList *waarschuwingen = nullptr);

    // Proton, Steam Linux Runtime en dergelijke zijn geen games
    static bool isGereedschap(const QString &appid, const QString &naam);

    // Het hoesje (600x900) uit de cache van Steam, of een ander plaatje als dat er niet is
    static QString vindCover(const QString &steamMap, const QString &appid);
};
