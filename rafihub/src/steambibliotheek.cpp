// RafiHub – steambibliotheek.cpp

#include "steambibliotheek.h"

#include "vdf.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>

QStringList SteamBibliotheek::kandidaatMappen(const QString &home)
{
    return {
        home + QStringLiteral("/.local/share/Steam"),
        home + QStringLiteral("/.steam/steam"),
        home + QStringLiteral("/.steam/root"),
        home + QStringLiteral("/.var/app/com.valvesoftware.Steam/.local/share/Steam"),
    };
}

QString SteamBibliotheek::vindSteamMap(const QString &home)
{
    for (const QString &map : kandidaatMappen(home)) {
        if (QFileInfo::exists(map + QStringLiteral("/steamapps"))) {
            return QFileInfo(map).canonicalFilePath();
        }
    }
    return {};
}

bool SteamBibliotheek::isGereedschap(const QString &appid, const QString &naam)
{
    static const QSet<QString> bekend = {
        QStringLiteral("228980"), // Steamworks Common Redistributables
    };
    return bekend.contains(appid) || naam.startsWith(QStringLiteral("Proton"))
        || naam.startsWith(QStringLiteral("Steam Linux Runtime"))
        || naam.startsWith(QStringLiteral("Steamworks Common"));
}

QString SteamBibliotheek::vindCover(const QString &steamMap, const QString &appid)
{
    const QString cache = steamMap + QStringLiteral("/appcache/librarycache");

    // Oude indeling: alles plat in één map
    const QString plat = cache + QStringLiteral("/%1_library_600x900.jpg").arg(appid);
    if (QFileInfo::exists(plat)) {
        return plat;
    }
    // Nieuwe indeling (sinds 2024): een map per game, soms met nog een submap
    const QString map = cache + QLatin1Char('/') + appid;
    if (QFileInfo::exists(map + QStringLiteral("/library_600x900.jpg"))) {
        return map + QStringLiteral("/library_600x900.jpg");
    }
    QString header;
    QDirIterator it(map, {QStringLiteral("*.jpg"), QStringLiteral("*.png")}, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString pad = it.next();
        const QString naam = it.fileName();
        if (naam.startsWith(QStringLiteral("library_600x900"))) {
            return pad;
        }
        if (header.isEmpty() && naam.startsWith(QStringLiteral("header"))) {
            header = pad;
        }
    }
    if (header.isEmpty()) {
        const QString platHeader = cache + QStringLiteral("/%1_header.jpg").arg(appid);
        if (QFileInfo::exists(platHeader)) {
            header = platHeader;
        }
    }
    return header;
}

// Alle bibliotheekmappen uit libraryfolders.vdf (nieuw en oud formaat)
static QStringList bibliotheken(const QString &steamMap, QStringList *waarschuwingen)
{
    QStringList uit{steamMap};
    const vdf::Resultaat r =
        vdf::leesBestand(steamMap + QStringLiteral("/steamapps/libraryfolders.vdf"));
    if (!r.ok()) {
        if (waarschuwingen && QFileInfo::exists(steamMap + QStringLiteral("/steamapps/libraryfolders.vdf"))) {
            waarschuwingen->append(r.fout);
        }
        return uit;
    }
    const QVariantMap mappen = vdf::zoek(r.data, {QStringLiteral("libraryfolders")}).toMap();
    for (auto it = mappen.constBegin(); it != mappen.constEnd(); ++it) {
        QString pad;
        if (it.value().typeId() == QMetaType::QVariantMap) {
            pad = it.value().toMap().value(QStringLiteral("path")).toString(); // nieuw
        } else {
            bool getal = false;
            it.key().toInt(&getal);
            if (getal) {
                pad = it.value().toString(); // oud: "1" "/pad"
            }
        }
        if (!pad.isEmpty()) {
            const QString echt = QFileInfo(pad).canonicalFilePath();
            const QString kandidaat = echt.isEmpty() ? pad : echt;
            if (!uit.contains(kandidaat)) {
                uit.append(kandidaat);
            }
        }
    }
    return uit;
}

QList<SteamGame> SteamBibliotheek::lees(const QString &steamMap, QStringList *waarschuwingen)
{
    QList<SteamGame> games;
    if (steamMap.isEmpty()) {
        return games;
    }
    QSet<QString> gezien;
    for (const QString &bieb : bibliotheken(steamMap, waarschuwingen)) {
        const QDir apps(bieb + QStringLiteral("/steamapps"));
        const QStringList manifesten =
            apps.entryList({QStringLiteral("appmanifest_*.acf")}, QDir::Files, QDir::Name);
        for (const QString &bestand : manifesten) {
            const vdf::Resultaat r = vdf::leesBestand(apps.filePath(bestand));
            if (!r.ok()) {
                if (waarschuwingen) {
                    waarschuwingen->append(r.fout);
                }
                continue;
            }
            const QVariantMap staat = vdf::zoek(r.data, {QStringLiteral("appstate")}).toMap();
            const QString appid = staat.value(QStringLiteral("appid")).toString();
            const QString naam = staat.value(QStringLiteral("name")).toString();
            const int vlaggen = staat.value(QStringLiteral("stateflags")).toInt();

            if (appid.isEmpty() || naam.isEmpty() || gezien.contains(appid)) {
                continue;
            }
            if ((vlaggen & 4) == 0 || isGereedschap(appid, naam)) {
                continue; // niet (helemaal) geïnstalleerd, of geen game
            }
            gezien.insert(appid);

            SteamGame g;
            g.appid = appid;
            g.naam = naam;
            g.installPad = apps.filePath(QStringLiteral("common/")
                                         + staat.value(QStringLiteral("installdir")).toString());
            g.laatstGespeeld = staat.value(QStringLiteral("lastplayed")).toLongLong();
            g.cover = vindCover(steamMap, appid);
            games.append(g);
        }
    }
    return games;
}
