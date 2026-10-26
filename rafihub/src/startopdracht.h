// RafiHub – startopdracht.h
// Bepaalt HOE een game gestart wordt. Bewust los van het echte starten,
// zodat het zonder games te testen is.

#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

enum class Bron { Steam, Eigen };

struct GameInfo {
    QString id;
    Bron bron = Bron::Eigen;
    QString appid; // Steam
    QString titel;
    QString pad; // eigen game
    bool mangohud = false;
};

struct StartOpdracht {
    QString programma;
    QStringList argumenten;
    QMap<QString, QString> extraOmgeving;
    QString werkmap;

    QString alsTekst() const; // voor in logs en tests
};

// benchmark = starten via rafi-bench (meet fps en vergelijkt met de vorige keer)
StartOpdracht maakStartOpdracht(const GameInfo &game, bool benchmark = false);

// "Counter-Strike 2" -> "counter-strike-2" (voor mapnamen van rafi-bench)
QString veiligeNaam(const QString &titel);

// Wat je in Steam bij Opstartopties zet om een Steam-game te benchmarken
QString steamBenchmarkOptie(const QString &titel);
