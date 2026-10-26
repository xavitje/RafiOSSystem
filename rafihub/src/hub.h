// RafiHub – hub.h
// Het "brein" achter de interface: laadt games, start ze, bewaart instellingen
// en houdt de status van RafiTune in de gaten.

#pragma once

#include "gamemodel.h"
#include "opslag.h"
#include "startopdracht.h"

#include <QObject>
#include <QTimer>
#include <functional>

class Hub : public QObject {
    Q_OBJECT
    Q_PROPERTY(GameFilter *games READ games CONSTANT)
    Q_PROPERTY(int totaal READ totaal NOTIFY geladen)
    Q_PROPERTY(QString steamMap READ steamMap NOTIFY geladen)
    Q_PROPERTY(QString rafitune READ rafitune NOTIFY rafituneVeranderd)
    Q_PROPERTY(bool rafituneActief READ rafituneActief NOTIFY rafituneVeranderd)
public:
    // Hoe een game echt gestart wordt. Tests vervangen dit, zodat er niets start.
    using Starter = std::function<bool(const StartOpdracht &, QString *fout)>;

    explicit Hub(const QString &opslagBestand, QObject *ouder = nullptr);

    void zetStarter(Starter s) { m_starter = std::move(s); }
    void zetSteamMap(const QString &map) { m_steamMapVast = map; }
    void zetStatusBestand(const QString &pad);

    GameFilter *games() { return &m_filter; }
    int totaal() const { return m_model.rowCount(); }
    QString steamMap() const { return m_steamMap; }
    QString rafitune() const { return m_rafitune; }
    bool rafituneActief() const { return m_rafituneActief; }

    Q_INVOKABLE void vernieuw();
    Q_INVOKABLE bool speel(const QString &id, bool benchmark = false);
    Q_INVOKABLE void zetMangohud(const QString &id, bool aan);
    Q_INVOKABLE QString voegToe(const QString &titel, const QString &padOfUrl,
                                const QString &coverOfUrl = {});
    Q_INVOKABLE bool verwijder(const QString &id);
    Q_INVOKABLE QString titelVoorPad(const QString &padOfUrl) const;
    Q_INVOKABLE QString benchmarkOptie(const QString &id) const;
    Q_INVOKABLE void kopieer(const QString &tekst);
    Q_INVOKABLE QString laatstTekst(qint64 unixTijd) const;

    static bool standaardStarter(const StartOpdracht &o, QString *fout);
    static QString naarPad(const QString &padOfUrl);

signals:
    void geladen();
    void rafituneVeranderd();
    void melding(const QString &tekst);

private:
    void leesRafitune();

    Opslag m_opslag;
    GameModel m_model;
    GameFilter m_filter;
    Starter m_starter = &Hub::standaardStarter;
    QString m_steamMapVast;
    QString m_steamMap;
    QString m_statusBestand = QStringLiteral("/run/rafitune/status");
    QString m_rafitune;
    bool m_rafituneActief = false;
    QTimer m_rafituneTimer;
};
