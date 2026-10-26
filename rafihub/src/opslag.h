// RafiHub – opslag.h
// Bewaart je eigen games (losse .exe's en Linux-programma's) en instellingen
// per game in ~/.config/rafihub/games.json.

#pragma once

#include <QHash>
#include <QList>
#include <QString>

struct EigenGame {
    QString id; // "eigen:<uuid>"
    QString titel;
    QString pad;   // .exe, .msi, script of programma
    QString cover; // eigen plaatje, mag leeg zijn
};

struct GameInstellingen {
    bool mangohud = false;
    qint64 laatstGespeeld = 0; // door RafiHub gestart (Unix-tijd)
};

class Opslag {
public:
    explicit Opslag(QString bestand = standaardBestand());

    static QString standaardBestand();

    // Bestaat het bestand niet, dan is dat geen fout (eerste keer).
    bool laad(QString *fout = nullptr);
    bool bewaar(QString *fout = nullptr) const;

    const QList<EigenGame> &eigenGames() const { return m_games; }
    EigenGame voegToe(const QString &titel, const QString &pad, const QString &cover = {});
    bool verwijder(const QString &id);

    GameInstellingen instellingen(const QString &id) const { return m_instellingen.value(id); }
    void zetInstellingen(const QString &id, const GameInstellingen &i) { m_instellingen.insert(id, i); }

    const QString &bestand() const { return m_bestand; }

private:
    QString m_bestand;
    QList<EigenGame> m_games;
    QHash<QString, GameInstellingen> m_instellingen;
};
