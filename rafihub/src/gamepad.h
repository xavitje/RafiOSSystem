// RafiHub – gamepad.h
// Controller-ondersteuning via SDL2, die pas bij het starten geladen wordt.
// Zonder SDL2 (of zonder controller) doet deze klasse niets, en werkt RafiHub
// met muis en toetsenbord.

#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

class Gamepad : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool verbonden READ verbonden NOTIFY verbondenVeranderd)
    Q_PROPERTY(QString naam READ naam NOTIFY verbondenVeranderd)
public:
    explicit Gamepad(QObject *ouder = nullptr);
    ~Gamepad() override;

    bool verbonden() const { return m_verbonden; }
    QString naam() const { return m_naam; }

signals:
    void richting(int dx, int dy); // d-pad of linker stick
    void bevestig();               // A (of Start)
    void terug();                  // B
    void extra();                  // X: benchmark
    void zoeken();                 // Y
    void verbondenVeranderd();

private:
    void controleer();
    void verwerkRichting(int dx, int dy);

    QTimer m_timer;
    QElapsedTimer m_klok;
    bool m_verbonden = false;
    QString m_naam;
    bool m_actief = false; // SDL2 geladen en gestart
    void *m_pad = nullptr; // SDL_GameController*
    quint32 m_knoppen = 0; // welke knoppen vorige keer ingedrukt waren
    int m_vorigeX = 0, m_vorigeY = 0;
    qint64 m_volgendeHerhaling = 0;
};
