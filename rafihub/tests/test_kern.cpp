// RafiHub – tests voor de kern (zonder scherm, zonder echte games).

#include "../src/gamemodel.h"
#include "../src/hub.h"
#include "../src/opslag.h"
#include "../src/startopdracht.h"
#include "../src/steambibliotheek.h"
#include "../src/vdf.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

static void schrijf(const QString &pad, const QByteArray &inhoud)
{
    QDir().mkpath(QFileInfo(pad).absolutePath());
    QFile f(pad);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(inhoud);
}

static QByteArray manifest(const QString &appid, const QString &naam, int vlaggen,
                           qint64 laatst = 0)
{
    return QStringLiteral("\"AppState\"\n{\n\t\"appid\"\t\t\"%1\"\n\t\"name\"\t\t\"%2\"\n"
                          "\t\"StateFlags\"\t\t\"%3\"\n\t\"installdir\"\t\t\"%2\"\n"
                          "\t\"LastPlayed\"\t\t\"%4\"\n}\n")
        .arg(appid, naam)
        .arg(vlaggen)
        .arg(laatst)
        .toUtf8();
}

class TestKern : public QObject {
    Q_OBJECT

private slots:
    // ---- VDF ----
    void vdfGewoon()
    {
        const auto r = vdf::lees(QStringLiteral(
            "\"AppState\"\n{\n  \"appid\" \"730\"\n  \"Name\" \"Counter-Strike 2\"\n"
            "  \"UserConfig\" { \"language\" \"dutch\" }\n}\n"));
        QVERIFY2(r.ok(), qPrintable(r.fout));
        QCOMPARE(vdf::zoek(r.data, {"AppState", "appid"}).toString(), QStringLiteral("730"));
        QCOMPARE(vdf::zoek(r.data, {"appstate", "name"}).toString(), QStringLiteral("Counter-Strike 2"));
        QCOMPARE(vdf::zoek(r.data, {"appstate", "userconfig", "language"}).toString(),
                 QStringLiteral("dutch"));
        QVERIFY(!vdf::zoek(r.data, {"appstate", "bestaatniet"}).isValid());
    }

    void vdfSpecialeTekens()
    {
        const auto r = vdf::lees(QStringLiteral(
            "// commentaar bovenaan\n\"a\" { \"pad\" \"C:\\\\Games\\\\x\" // achteraan\n"
            "\"citaat\" \"zei \\\"hoi\\\"\" \"leeg\" \"\" \"[$WIN32]\" \"x\" zonder aanhalingstekens }"));
        QVERIFY2(r.ok(), qPrintable(r.fout));
        QCOMPARE(vdf::zoek(r.data, {"a", "pad"}).toString(), QStringLiteral("C:\\Games\\x"));
        QCOMPARE(vdf::zoek(r.data, {"a", "citaat"}).toString(), QStringLiteral("zei \"hoi\""));
        QCOMPARE(vdf::zoek(r.data, {"a", "leeg"}).toString(), QString());
        QCOMPARE(vdf::zoek(r.data, {"a", "zonder"}).toString(), QStringLiteral("aanhalingstekens"));
    }

    void vdfFouten()
    {
        QVERIFY(!vdf::lees(QStringLiteral("\"a\" { \"b\" \"c\"")).ok());   // } mist
        QVERIFY(!vdf::lees(QStringLiteral("\"a\" \"b\" }")).ok());         // } te veel
        QVERIFY(!vdf::lees(QStringLiteral("\"a\" { \"b\" \"nooit dicht")).ok());
        QVERIFY(!vdf::lees(QStringLiteral("{ \"b\" \"c\" }")).ok());        // geen naam
        QVERIFY(!vdf::lees(QStringLiteral("\"a\"")).ok());                 // waarde mist
        QVERIFY(!vdf::leesBestand(QStringLiteral("/bestaat/niet.vdf")).ok());
        QString diep;
        for (int i = 0; i < 100; ++i) {
            diep += QStringLiteral("\"a\" { ");
        }
        QVERIFY(!vdf::lees(diep).ok()); // geen crash bij extreem diep nesten
    }

    // ---- Steam-bibliotheek ----
    void steamBibliotheek()
    {
        QTemporaryDir tmp;
        const QString steam = tmp.path() + QStringLiteral("/home/.local/share/Steam");
        const QString tweede = tmp.path() + QStringLiteral("/SSD2/SteamLibrary");
        schrijf(steam + "/steamapps/libraryfolders.vdf",
                QStringLiteral("\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"%1\"\n\t}\n"
                               "\t\"1\"\n\t{\n\t\t\"path\"\t\t\"%2\"\n\t\t\"apps\" { \"570\" \"123\" }\n\t}\n}\n")
                    .arg(steam, tweede)
                    .toUtf8());
        schrijf(steam + "/steamapps/appmanifest_730.acf", manifest("730", "Counter-Strike 2", 4, 1700000000));
        schrijf(steam + "/steamapps/appmanifest_1493710.acf", manifest("1493710", "Proton Experimental", 4));
        schrijf(steam + "/steamapps/appmanifest_228980.acf", manifest("228980", "Steamworks Common Redistributables", 4));
        schrijf(steam + "/steamapps/appmanifest_999.acf", manifest("999", "Half gedownload", 1026));
        schrijf(steam + "/steamapps/appmanifest_kapot.acf", "\"AppState\" { \"appid\"");
        schrijf(tweede + "/steamapps/appmanifest_570.acf", manifest("570", "Dota 2", 6));
        // Covers: oude en nieuwe indeling
        schrijf(steam + "/appcache/librarycache/730_library_600x900.jpg", "jpg");
        schrijf(steam + "/appcache/librarycache/570/abc123/library_600x900.jpg", "jpg");

        QCOMPARE(SteamBibliotheek::vindSteamMap(tmp.path() + "/home"), QFileInfo(steam).canonicalFilePath());
        QVERIFY(SteamBibliotheek::vindSteamMap(tmp.path() + "/leeg").isEmpty());

        QStringList waarschuwingen;
        const QList<SteamGame> games = SteamBibliotheek::lees(steam, &waarschuwingen);
        QStringList namen;
        for (const SteamGame &g : games) {
            namen << g.naam;
        }
        namen.sort();
        QCOMPARE(namen, (QStringList{"Counter-Strike 2", "Dota 2"}));
        QCOMPARE(waarschuwingen.size(), 1); // het kapotte manifest
        for (const SteamGame &g : games) {
            if (g.appid == "730") {
                QCOMPARE(g.laatstGespeeld, qint64(1700000000));
                QVERIFY(g.cover.endsWith("730_library_600x900.jpg"));
            } else {
                QVERIFY2(g.cover.endsWith("570/abc123/library_600x900.jpg"), qPrintable(g.cover));
            }
        }
    }

    void steamOudFormaatEnGeenSteam()
    {
        QTemporaryDir tmp;
        const QString steam = tmp.path() + "/Steam";
        const QString tweede = tmp.path() + "/Extra";
        schrijf(steam + "/steamapps/libraryfolders.vdf",
                QStringLiteral("\"LibraryFolders\" { \"TimeNextStatsReport\" \"1\" \"1\" \"%1\" }").arg(tweede).toUtf8());
        schrijf(tweede + "/steamapps/appmanifest_10.acf", manifest("10", "Oude game", 4));
        QCOMPARE(SteamBibliotheek::lees(steam).size(), 1);
        QVERIFY(SteamBibliotheek::lees(QString()).isEmpty());
        QVERIFY(SteamBibliotheek::lees(tmp.path() + "/bestaatniet").isEmpty());
    }

    // ---- Opslag ----
    void opslagHeenEnTerug()
    {
        QTemporaryDir tmp;
        const QString bestand = tmp.path() + "/config/rafihub/games.json";
        Opslag a(bestand);
        QVERIFY(a.laad()); // bestaat nog niet: geen fout
        const EigenGame g = a.voegToe(QString(), "/games/Witcher 3/witcher3.exe");
        QCOMPARE(g.titel, QStringLiteral("witcher3")); // titel uit bestandsnaam
        QVERIFY(g.id.startsWith("eigen:"));
        a.zetInstellingen(g.id, {true, 1234});
        a.zetInstellingen("steam:730", {false, 99});
        QString fout;
        QVERIFY2(a.bewaar(&fout), qPrintable(fout));

        Opslag b(bestand);
        QVERIFY(b.laad(&fout));
        QCOMPARE(b.eigenGames().size(), 1);
        QCOMPARE(b.eigenGames().first().pad, QStringLiteral("/games/Witcher 3/witcher3.exe"));
        QCOMPARE(b.instellingen(g.id).mangohud, true);
        QCOMPARE(b.instellingen(g.id).laatstGespeeld, qint64(1234));
        QCOMPARE(b.instellingen("steam:730").laatstGespeeld, qint64(99));
        QVERIFY(b.verwijder(g.id));
        QVERIFY(!b.verwijder(g.id));
        QCOMPARE(b.instellingen(g.id).mangohud, false);
    }

    void opslagBeschadigd()
    {
        QTemporaryDir tmp;
        schrijf(tmp.path() + "/games.json", "{ dit is geen json");
        Opslag o(tmp.path() + "/games.json");
        QString fout;
        QVERIFY(!o.laad(&fout));
        QVERIFY(fout.contains("beschadigd"));
        QVERIFY(o.eigenGames().isEmpty());
    }

    // ---- Startopdracht ----
    void startOpdrachten()
    {
        GameInfo steam{"steam:730", Bron::Steam, "730", "Counter-Strike 2", {}, true};
        StartOpdracht o = maakStartOpdracht(steam);
        QCOMPARE(o.programma, QStringLiteral("steam"));
        QCOMPARE(o.argumenten, QStringList{"steam://rungameid/730"});

        GameInfo exe{"eigen:1", Bron::Eigen, {}, "Witcher 3", "/g/Witcher 3/Witcher3.EXE", false};
        o = maakStartOpdracht(exe);
        QCOMPARE(o.programma, QStringLiteral("rafi-exe"));
        QCOMPARE(o.argumenten, QStringList{"/g/Witcher 3/Witcher3.EXE"});
        QCOMPARE(o.werkmap, QStringLiteral("/g/Witcher 3"));
        QVERIFY(o.extraOmgeving.isEmpty());

        exe.pad = "/g/setup.msi";
        exe.mangohud = true;
        o = maakStartOpdracht(exe);
        QCOMPARE(o.programma, QStringLiteral("rafi-exe"));
        QCOMPARE(o.extraOmgeving.value("MANGOHUD"), QStringLiteral("1"));

        GameInfo linuxGame{"eigen:2", Bron::Eigen, {}, "Mijn Spel: Deel 2!", "/opt/spel/start.sh", true};
        o = maakStartOpdracht(linuxGame, true);
        QCOMPARE(o.programma, QStringLiteral("rafi-bench"));
        QCOMPARE(o.argumenten, (QStringList{"mijn-spel-deel-2", "--", "/opt/spel/start.sh"}));
        QVERIFY(o.extraOmgeving.isEmpty()); // rafi-bench regelt MangoHud zelf
        QCOMPARE(o.werkmap, QStringLiteral("/opt/spel"));

        exe.pad = "/g/w3.exe";
        o = maakStartOpdracht(exe, true);
        QCOMPARE(o.argumenten, (QStringList{"witcher-3", "--", "rafi-exe", "/g/w3.exe"}));
        QCOMPARE(o.alsTekst(), QStringLiteral("rafi-bench witcher-3 -- rafi-exe /g/w3.exe"));
    }

    void namen()
    {
        QCOMPARE(veiligeNaam("Counter-Strike 2"), QStringLiteral("counter-strike-2"));
        QCOMPARE(veiligeNaam("  !!  "), QStringLiteral("game"));
        QCOMPARE(veiligeNaam("Élan --- Vital"), QStringLiteral("lan-vital"));
        QCOMPARE(steamBenchmarkOptie("Counter-Strike 2"),
                 QStringLiteral("rafi-bench counter-strike-2 -- %command%"));
        QCOMPARE(GameModel::letters("Counter-Strike 2"), QStringLiteral("C2"));
        QCOMPARE(GameModel::letters("dota"), QStringLiteral("D"));
        QCOMPARE(GameModel::letters(""), QStringLiteral("?"));
        QCOMPARE(GameModel::kleur("Dota 2"), GameModel::kleur("Dota 2")); // altijd dezelfde
    }

    // ---- Hub (alles samen) ----
    void hub()
    {
        QTemporaryDir tmp;
        const QString steam = tmp.path() + "/Steam";
        schrijf(steam + "/steamapps/appmanifest_730.acf", manifest("730", "Counter-Strike 2", 4, 100));
        schrijf(steam + "/steamapps/appmanifest_570.acf", manifest("570", "Dota 2", 4, 200));
        schrijf(tmp.path() + "/status", "actief=ja\ngame=Dota 2\n");
        const QString exe = tmp.path() + "/games/My_Cool-Game.exe";
        schrijf(exe, "MZ");
        const QString tekst = tmp.path() + "/games/leesmij.txt";
        schrijf(tekst, "geen programma");

        QList<StartOpdracht> gestart;
        Hub hub(tmp.path() + "/games.json");
        hub.zetSteamMap(steam);
        hub.zetStatusBestand(tmp.path() + "/status");
        hub.zetStarter([&](const StartOpdracht &o, QString *) {
            gestart << o;
            return true;
        });
        hub.vernieuw();

        QCOMPARE(hub.totaal(), 2);
        QCOMPARE(hub.rafitune(), QStringLiteral("RafiTune optimaliseert Dota 2"));
        QVERIFY(hub.rafituneActief());
        GameFilter *f = hub.games();
        QCOMPARE(f->get(0).value("titel").toString(), QStringLiteral("Dota 2")); // laatst gespeeld eerst

        // Toevoegen: fout bestand, dan een goede .exe
        QVERIFY(!hub.voegToe("x", tekst).isEmpty());
        QVERIFY(!hub.voegToe("x", tmp.path() + "/bestaatniet.exe").isEmpty());
        QCOMPARE(hub.titelVoorPad(QUrl::fromLocalFile(exe).toString()), QStringLiteral("My Cool Game"));
        QCOMPARE(hub.voegToe("", QUrl::fromLocalFile(exe).toString()), QString());
        QCOMPARE(hub.totaal(), 3);

        // Filteren en zoeken
        f->zetBron("eigen");
        QCOMPARE(f->aantal(), 1);
        const QString eigenId = f->get(0).value("gameId").toString();
        f->zetBron("alles");
        f->zetZoekTekst("counter");
        QCOMPARE(f->aantal(), 1);
        f->zetZoekTekst("");
        QCOMPARE(f->aantal(), 3);

        // Starten: Steam en eigen game, met MangoHud
        QVERIFY(hub.speel("steam:730"));
        QCOMPARE(gestart.last().argumenten, QStringList{"steam://rungameid/730"});
        QCOMPARE(f->get(0).value("gameId").toString(), QStringLiteral("steam:730")); // nu bovenaan
        hub.zetMangohud(eigenId, true);
        QVERIFY(hub.speel(eigenId));
        QCOMPARE(gestart.last().programma, QStringLiteral("rafi-exe"));
        QCOMPARE(gestart.last().extraOmgeving.value("MANGOHUD"), QStringLiteral("1"));
        QVERIFY(hub.speel(eigenId, true));
        QCOMPARE(gestart.last().programma, QStringLiteral("rafi-bench"));

        // Steam-benchmark start niets, maar geeft de opstartoptie
        QSignalSpy meldingen(&hub, &Hub::melding);
        const int voor = static_cast<int>(gestart.size());
        QVERIFY(hub.speel("steam:570", true));
        QCOMPARE(gestart.size(), voor);
        QVERIFY(meldingen.last().first().toString().contains("rafi-bench dota-2 -- %command%"));

        // Instellingen blijven bewaard na opnieuw laden
        Hub hub2(tmp.path() + "/games.json");
        hub2.zetSteamMap(steam);
        hub2.vernieuw();
        QCOMPARE(hub2.totaal(), 3);
        QVERIFY(hub2.games()->get(hub2.games()->rijVan(eigenId)).value("mangohud").toBool());

        // Starten mislukt netjes
        hub.zetStarter([](const StartOpdracht &, QString *fout) {
            *fout = "niet gevonden";
            return false;
        });
        QVERIFY(!hub.speel("steam:730"));
        QVERIFY(meldingen.last().first().toString().contains("niet gevonden"));
        QVERIFY(!hub.speel("bestaat:niet"));

        // Verwijderen
        QVERIFY(hub.verwijder(eigenId));
        QCOMPARE(hub.totaal(), 2);

        // Zonder RafiTune
        hub.zetStatusBestand(tmp.path() + "/geen-status");
        QVERIFY(!hub.rafituneActief());
        QCOMPARE(hub.rafitune(), QStringLiteral("RafiTune draait niet"));
    }

    void laatstTekst()
    {
        Hub hub(QStringLiteral("/dev/null/x.json"));
        QCOMPARE(hub.laatstTekst(0), QStringLiteral("nog niet gespeeld"));
        QCOMPARE(hub.laatstTekst(QDateTime::currentSecsSinceEpoch()), QStringLiteral("vandaag"));
        QCOMPARE(hub.laatstTekst(QDateTime::currentDateTime().addDays(-3).toSecsSinceEpoch()),
                 QStringLiteral("3 dagen geleden"));
    }
};

QTEST_MAIN(TestKern)
#include "test_kern.moc"
