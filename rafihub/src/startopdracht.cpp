// RafiHub – startopdracht.cpp

#include "startopdracht.h"

#include <QFileInfo>
#include <QRegularExpression>

QString StartOpdracht::alsTekst() const
{
    QStringList delen;
    for (auto it = extraOmgeving.constBegin(); it != extraOmgeving.constEnd(); ++it) {
        delen << it.key() + QLatin1Char('=') + it.value();
    }
    delen << programma;
    for (const QString &a : argumenten) {
        delen << (a.contains(QLatin1Char(' ')) ? QLatin1Char('\'') + a + QLatin1Char('\'') : a);
    }
    return delen.join(QLatin1Char(' '));
}

QString veiligeNaam(const QString &titel)
{
    QString n = titel.toLower();
    n.replace(QRegularExpression(QStringLiteral("[^a-z0-9._-]+")), QStringLiteral("-"));
    n.replace(QRegularExpression(QStringLiteral("-{2,}")), QStringLiteral("-"));
    while (n.startsWith(QLatin1Char('-'))) {
        n.remove(0, 1);
    }
    while (n.endsWith(QLatin1Char('-'))) {
        n.chop(1);
    }
    return n.isEmpty() ? QStringLiteral("game") : n;
}

QString steamBenchmarkOptie(const QString &titel)
{
    return QStringLiteral("rafi-bench %1 -- %command%").arg(veiligeNaam(titel));
}

StartOpdracht maakStartOpdracht(const GameInfo &game, bool benchmark)
{
    StartOpdracht o;

    if (game.bron == Bron::Steam) {
        // Steam start de game zelf (met Proton als dat nodig is).
        // Benchmarken gaat via de opstartopties in Steam: zie steamBenchmarkOptie().
        o.programma = QStringLiteral("steam");
        o.argumenten = {QStringLiteral("steam://rungameid/") + game.appid};
        return o;
    }

    const QString kleineLetters = game.pad.toLower();
    if (kleineLetters.endsWith(QStringLiteral(".exe")) || kleineLetters.endsWith(QStringLiteral(".msi"))) {
        o.programma = QStringLiteral("rafi-exe"); // Proton, eigen Windows-map per game
        o.argumenten = {game.pad};
    } else {
        o.programma = game.pad; // Linux-programma of script
    }
    o.werkmap = QFileInfo(game.pad).absolutePath();

    if (benchmark) {
        // rafi-bench zet MangoHud zelf aan en bewaart het resultaat
        QStringList args{veiligeNaam(game.titel), QStringLiteral("--"), o.programma};
        args += o.argumenten;
        o.programma = QStringLiteral("rafi-bench");
        o.argumenten = args;
    } else if (game.mangohud) {
        o.extraOmgeving.insert(QStringLiteral("MANGOHUD"), QStringLiteral("1"));
    }
    return o;
}
