// RafiHub – hub.cpp

#include "hub.h"

#include "steambibliotheek.h"

#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QUrl>

Hub::Hub(const QString &opslagBestand, QObject *ouder)
    : QObject(ouder), m_opslag(opslagBestand)
{
    m_filter.setSourceModel(&m_model);
    connect(&m_rafituneTimer, &QTimer::timeout, this, &Hub::leesRafitune);
    m_rafituneTimer.start(3000);
    leesRafitune();
}

void Hub::zetStatusBestand(const QString &pad)
{
    m_statusBestand = pad;
    leesRafitune();
}

void Hub::vernieuw()
{
    QString fout;
    if (!m_opslag.laad(&fout)) {
        emit melding(fout);
    }

    QList<GameRij> rijen;

    m_steamMap = m_steamMapVast.isEmpty() ? SteamBibliotheek::vindSteamMap(QDir::homePath())
                                          : m_steamMapVast;
    QStringList waarschuwingen;
    for (const SteamGame &s : SteamBibliotheek::lees(m_steamMap, &waarschuwingen)) {
        GameRij r;
        r.info.id = QStringLiteral("steam:") + s.appid;
        r.info.bron = Bron::Steam;
        r.info.appid = s.appid;
        r.info.titel = s.naam;
        r.cover = s.cover;
        const GameInstellingen inst = m_opslag.instellingen(r.info.id);
        r.info.mangohud = inst.mangohud;
        r.laatstGespeeld = qMax(s.laatstGespeeld, inst.laatstGespeeld);
        rijen.append(r);
    }
    for (const QString &w : waarschuwingen) {
        qWarning("rafihub: %s", qPrintable(w));
    }

    for (const EigenGame &e : m_opslag.eigenGames()) {
        GameRij r;
        r.info.id = e.id;
        r.info.bron = Bron::Eigen;
        r.info.titel = e.titel;
        r.info.pad = e.pad;
        r.cover = e.cover;
        const GameInstellingen inst = m_opslag.instellingen(e.id);
        r.info.mangohud = inst.mangohud;
        r.laatstGespeeld = inst.laatstGespeeld;
        rijen.append(r);
    }

    m_model.zet(rijen);
    emit geladen();
}

bool Hub::speel(const QString &id, bool benchmark)
{
    const GameRij *r = m_model.zoek(id);
    if (r == nullptr) {
        emit melding(QStringLiteral("Game niet gevonden"));
        return false;
    }
    if (benchmark && r->info.bron == Bron::Steam) {
        // Een Steam-game benchmark je via de opstartopties in Steam
        kopieer(steamBenchmarkOptie(r->info.titel));
        emit melding(QStringLiteral("Gekopieerd: %1 — plak dit in Steam bij Eigenschappen > "
                                    "Opstartopties en start de game daarna gewoon.")
                         .arg(steamBenchmarkOptie(r->info.titel)));
        return true;
    }
    if (r->info.bron == Bron::Eigen && !QFileInfo::exists(r->info.pad)) {
        emit melding(QStringLiteral("%1 bestaat niet meer").arg(r->info.pad));
        return false;
    }

    const GameInfo info = r->info; // kopie: het model kan hieronder veranderen
    const StartOpdracht o = maakStartOpdracht(info, benchmark);
    QString fout;
    if (!m_starter(o, &fout)) {
        emit melding(QStringLiteral("Kan %1 niet starten: %2").arg(info.titel, fout));
        return false;
    }

    GameInstellingen inst = m_opslag.instellingen(id);
    inst.laatstGespeeld = QDateTime::currentSecsSinceEpoch();
    m_opslag.zetInstellingen(id, inst);
    if (!m_opslag.bewaar(&fout)) {
        emit melding(fout);
    }
    m_model.werkBij(id, info.mangohud, inst.laatstGespeeld);
    emit melding(benchmark ? QStringLiteral("%1 start met benchmark").arg(info.titel)
                           : QStringLiteral("%1 start…").arg(info.titel));
    return true;
}

void Hub::zetMangohud(const QString &id, bool aan)
{
    const GameRij *r = m_model.zoek(id);
    if (r == nullptr) {
        return;
    }
    GameInstellingen inst = m_opslag.instellingen(id);
    inst.mangohud = aan;
    m_opslag.zetInstellingen(id, inst);
    QString fout;
    if (!m_opslag.bewaar(&fout)) {
        emit melding(fout);
    }
    m_model.werkBij(id, aan, r->laatstGespeeld);
}

QString Hub::naarPad(const QString &padOfUrl)
{
    if (padOfUrl.startsWith(QStringLiteral("file:"))) {
        return QUrl(padOfUrl).toLocalFile();
    }
    return padOfUrl;
}

QString Hub::voegToe(const QString &titel, const QString &padOfUrl, const QString &coverOfUrl)
{
    const QString pad = naarPad(padOfUrl);
    const QFileInfo info(pad);
    if (pad.isEmpty() || !info.isFile()) {
        return QStringLiteral("Kies eerst een bestand.");
    }
    const QString klein = pad.toLower();
    const bool windows = klein.endsWith(QStringLiteral(".exe")) || klein.endsWith(QStringLiteral(".msi"));
    if (!windows && !info.isExecutable()) {
        return QStringLiteral("Dit bestand kan niet gestart worden. Kies een .exe, .msi of een "
                              "uitvoerbaar programma.");
    }
    const QString cover = naarPad(coverOfUrl);
    const EigenGame g = m_opslag.voegToe(titel, info.absoluteFilePath(), cover);
    QString fout;
    if (!m_opslag.bewaar(&fout)) {
        return fout;
    }
    vernieuw();
    emit melding(QStringLiteral("%1 toegevoegd").arg(g.titel));
    return {};
}

bool Hub::verwijder(const QString &id)
{
    if (!m_opslag.verwijder(id)) {
        return false;
    }
    QString fout;
    if (!m_opslag.bewaar(&fout)) {
        emit melding(fout);
    }
    vernieuw();
    emit melding(QStringLiteral("Uit RafiHub gehaald (het bestand zelf staat er nog)"));
    return true;
}

QString Hub::titelVoorPad(const QString &padOfUrl) const
{
    QString naam = QFileInfo(naarPad(padOfUrl)).completeBaseName();
    naam.replace(QLatin1Char('_'), QLatin1Char(' '));
    naam.replace(QLatin1Char('-'), QLatin1Char(' '));
    if (!naam.isEmpty()) {
        naam[0] = naam.at(0).toUpper();
    }
    return naam.simplified();
}

QString Hub::benchmarkOptie(const QString &id) const
{
    const GameRij *r = m_model.zoek(id);
    return r ? steamBenchmarkOptie(r->info.titel) : QString();
}

void Hub::kopieer(const QString &tekst)
{
    if (QGuiApplication::clipboard()) {
        QGuiApplication::clipboard()->setText(tekst);
    }
}

QString Hub::laatstTekst(qint64 unixTijd) const
{
    if (unixTijd <= 0) {
        return QStringLiteral("nog niet gespeeld");
    }
    const QDate dan = QDateTime::fromSecsSinceEpoch(unixTijd).date();
    const qint64 dagen = dan.daysTo(QDate::currentDate());
    if (dagen <= 0) {
        return QStringLiteral("vandaag");
    }
    if (dagen == 1) {
        return QStringLiteral("gisteren");
    }
    if (dagen < 30) {
        return QStringLiteral("%1 dagen geleden").arg(dagen);
    }
    return dan.toString(QStringLiteral("d-M-yyyy"));
}

bool Hub::standaardStarter(const StartOpdracht &o, QString *fout)
{
    QProcess p;
    p.setProgram(o.programma);
    p.setArguments(o.argumenten);
    if (!o.werkmap.isEmpty()) {
        p.setWorkingDirectory(o.werkmap);
    }
    QProcessEnvironment omgeving = QProcessEnvironment::systemEnvironment();
    for (auto it = o.extraOmgeving.constBegin(); it != o.extraOmgeving.constEnd(); ++it) {
        omgeving.insert(it.key(), it.value());
    }
    p.setProcessEnvironment(omgeving);
    // Los starten: de game blijft draaien als RafiHub sluit
    if (!p.startDetached()) {
        if (fout) {
            *fout = QStringLiteral("'%1' niet gevonden of niet uitvoerbaar").arg(o.programma);
        }
        return false;
    }
    return true;
}

void Hub::leesRafitune()
{
    QString tekst;
    bool actief = false;
    QFile f(m_statusBestand);
    if (!f.open(QIODevice::ReadOnly)) {
        tekst = QStringLiteral("RafiTune draait niet");
    } else {
        QMap<QString, QString> waarden;
        for (const QByteArray &regel : f.readAll().split('\n')) {
            const int is = regel.indexOf('=');
            if (is > 0) {
                waarden.insert(QString::fromUtf8(regel.left(is)), QString::fromUtf8(regel.mid(is + 1)));
            }
        }
        actief = waarden.value(QStringLiteral("actief")) == QStringLiteral("ja");
        tekst = actief ? QStringLiteral("RafiTune optimaliseert %1").arg(waarden.value(QStringLiteral("game")))
                       : QStringLiteral("RafiTune staat klaar");
    }
    if (tekst != m_rafitune || actief != m_rafituneActief) {
        m_rafitune = tekst;
        m_rafituneActief = actief;
        emit rafituneVeranderd();
    }
}
