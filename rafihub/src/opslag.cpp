// RafiHub – opslag.cpp

#include "opslag.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

Opslag::Opslag(QString bestand) : m_bestand(std::move(bestand)) {}

QString Opslag::standaardBestand()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/rafihub/games.json");
}

bool Opslag::laad(QString *fout)
{
    m_games.clear();
    m_instellingen.clear();

    QFile f(m_bestand);
    if (!f.exists()) {
        return true;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        if (fout) {
            *fout = QStringLiteral("kan %1 niet openen").arg(m_bestand);
        }
        return false;
    }
    QJsonParseError fp;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &fp);
    if (fp.error != QJsonParseError::NoError || !doc.isObject()) {
        if (fout) {
            *fout = QStringLiteral("%1 is beschadigd: %2").arg(m_bestand, fp.errorString());
        }
        return false;
    }

    const QJsonObject wortel = doc.object();
    for (const QJsonValue &v : wortel.value(QStringLiteral("games")).toArray()) {
        const QJsonObject o = v.toObject();
        EigenGame g;
        g.id = o.value(QStringLiteral("id")).toString();
        g.titel = o.value(QStringLiteral("titel")).toString();
        g.pad = o.value(QStringLiteral("pad")).toString();
        g.cover = o.value(QStringLiteral("cover")).toString();
        if (!g.id.isEmpty() && !g.pad.isEmpty()) {
            m_games.append(g);
        }
    }
    const QJsonObject inst = wortel.value(QStringLiteral("instellingen")).toObject();
    for (auto it = inst.constBegin(); it != inst.constEnd(); ++it) {
        const QJsonObject o = it.value().toObject();
        GameInstellingen i;
        i.mangohud = o.value(QStringLiteral("mangohud")).toBool();
        i.laatstGespeeld = static_cast<qint64>(o.value(QStringLiteral("laatstGespeeld")).toDouble());
        m_instellingen.insert(it.key(), i);
    }
    return true;
}

bool Opslag::bewaar(QString *fout) const
{
    QJsonArray games;
    for (const EigenGame &g : m_games) {
        games.append(QJsonObject{{QStringLiteral("id"), g.id},
                                 {QStringLiteral("titel"), g.titel},
                                 {QStringLiteral("pad"), g.pad},
                                 {QStringLiteral("cover"), g.cover}});
    }
    QJsonObject inst;
    for (auto it = m_instellingen.constBegin(); it != m_instellingen.constEnd(); ++it) {
        inst.insert(it.key(),
                    QJsonObject{{QStringLiteral("mangohud"), it.value().mangohud},
                                {QStringLiteral("laatstGespeeld"),
                                 static_cast<double>(it.value().laatstGespeeld)}});
    }
    const QJsonObject wortel{{QStringLiteral("versie"), 1},
                             {QStringLiteral("games"), games},
                             {QStringLiteral("instellingen"), inst}};

    QDir().mkpath(QFileInfo(m_bestand).absolutePath());
    // QSaveFile schrijft eerst naar een tijdelijk bestand: nooit een half bestand
    QSaveFile f(m_bestand);
    if (!f.open(QIODevice::WriteOnly)
        || f.write(QJsonDocument(wortel).toJson(QJsonDocument::Indented)) < 0 || !f.commit()) {
        if (fout) {
            *fout = QStringLiteral("kan %1 niet opslaan: %2").arg(m_bestand, f.errorString());
        }
        return false;
    }
    return true;
}

EigenGame Opslag::voegToe(const QString &titel, const QString &pad, const QString &cover)
{
    EigenGame g;
    g.id = QStringLiteral("eigen:") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    g.titel = titel.trimmed().isEmpty() ? QFileInfo(pad).completeBaseName() : titel.trimmed();
    g.pad = pad;
    g.cover = cover;
    m_games.append(g);
    return g;
}

bool Opslag::verwijder(const QString &id)
{
    for (int i = 0; i < m_games.size(); ++i) {
        if (m_games.at(i).id == id) {
            m_games.removeAt(i);
            m_instellingen.remove(id);
            return true;
        }
    }
    return false;
}
