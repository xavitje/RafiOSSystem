// RafiHub – main.cpp
//
//   rafihub                     gewoon starten
//   rafihub --zelftest          interface laden zonder scherm; faalt bij QML-fouten
//   rafihub --screenshot x.png  interface tekenen en als plaatje opslaan
//
// Voor tests kun je Steam en RafiTune "nadoen" met:
//   RAFIHUB_STEAM_MAP=<map>  RAFIHUB_RAFITUNE_STATUS=<bestand>  XDG_CONFIG_HOME=<map>

#include "gamepad.h"
#include "hub.h"
#include "opslag.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

static int qmlFouten = 0;
static QtMessageHandler vorigeHandler = nullptr;

// Telt waarschuwingen die uit onze QML-bestanden komen (voor --zelftest)
static void telQmlFouten(QtMsgType soort, const QMessageLogContext &ctx, const QString &tekst)
{
    if (soort != QtDebugMsg && soort != QtInfoMsg
        && (tekst.contains(QStringLiteral("qrc:/")) || tekst.contains(QStringLiteral(".qml")))) {
        ++qmlFouten;
    }
    if (vorigeHandler) {
        vorigeHandler(soort, ctx, tekst);
    }
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("rafihub"));
    app.setApplicationDisplayName(QStringLiteral("RafiHub"));
    app.setApplicationVersion(QStringLiteral(RAFIHUB_VERSIE));
    app.setOrganizationName(QStringLiteral("RafiOS"));
    app.setDesktopFileName(QStringLiteral("rafihub"));
    app.setWindowIcon(QIcon(QStringLiteral(":/rafihub.svg")));

    QCommandLineParser opties;
    opties.setApplicationDescription(QStringLiteral("RafiHub – de game-launcher van RafiOS"));
    opties.addHelpOption();
    opties.addVersionOption();
    const QCommandLineOption zelftest(QStringLiteral("zelftest"),
                                      QStringLiteral("Interface laden en controleren, dan stoppen."));
    const QCommandLineOption screenshot(QStringLiteral("screenshot"),
                                        QStringLiteral("Interface opslaan als plaatje."),
                                        QStringLiteral("bestand"));
    opties.addOption(zelftest);
    opties.addOption(screenshot);
    opties.process(app);

    // Eigen stijl, zodat RafiHub er overal hetzelfde uitziet
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    if (opties.isSet(zelftest)) {
        vorigeHandler = qInstallMessageHandler(telQmlFouten);
    }

    Hub hub(Opslag::standaardBestand());
    if (qEnvironmentVariableIsSet("RAFIHUB_STEAM_MAP")) {
        hub.zetSteamMap(qEnvironmentVariable("RAFIHUB_STEAM_MAP"));
    }
    if (qEnvironmentVariableIsSet("RAFIHUB_RAFITUNE_STATUS")) {
        hub.zetStatusBestand(qEnvironmentVariable("RAFIHUB_RAFITUNE_STATUS"));
    }
    if (opties.isSet(zelftest) || opties.isSet(screenshot)) {
        // Nooit echt iets starten tijdens een test
        hub.zetStarter([](const StartOpdracht &o, QString *) {
            qInfo("rafihub: (test) zou starten: %s", qPrintable(o.alsTekst()));
            return true;
        });
    }
    hub.vernieuw();

    Gamepad gamepad;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("hub"), &hub);
    engine.rootContext()->setContextProperty(QStringLiteral("gamepad"), &gamepad);
    engine.rootContext()->setContextProperty(QStringLiteral("zelftestModus"), opties.isSet(zelftest));
    // Alleen voor screenshots van het toevoegvenster
    engine.rootContext()->setContextProperty(QStringLiteral("openToevoegen"),
                                             qEnvironmentVariableIsSet("RAFIHUB_OPEN_TOEVOEGEN"));
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    auto *venster = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());

    if (opties.isSet(zelftest)) {
        QTimer::singleShot(1500, &app, [&app] {
            if (qmlFouten > 0) {
                qCritical("rafihub: zelftest MISLUKT: %d QML-fout(en)", qmlFouten);
                app.exit(1);
            } else {
                qInfo("rafihub: zelftest geslaagd");
                app.exit(0);
            }
        });
    } else if (opties.isSet(screenshot)) {
        const QString bestand = opties.value(screenshot);
        QTimer::singleShot(1500, &app, [&app, venster, bestand] {
            const QImage beeld = venster ? venster->grabWindow() : QImage();
            const bool gelukt = !beeld.isNull() && beeld.save(bestand);
            qInfo("rafihub: screenshot %s: %s", gelukt ? "opgeslagen" : "MISLUKT", qPrintable(bestand));
            app.exit(gelukt ? 0 : 1);
        });
    }
    return app.exec();
}
