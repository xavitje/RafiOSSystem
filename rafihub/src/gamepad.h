// RafiHub – gamepad.h
// Controller-ondersteuning via SDL2. Zonder SDL2 (of zonder controller) doet
// deze klasse gewoon niets, en werkt RafiHub met muis en toetsenbord.

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
    void *m_pad = nullptr; // SDL_GameController*
    int m_dx = 0, m_dy = 0;         // knoppen (d-pad)
    int m_stickX = 0, m_stickY = 0; // linker stick
    int m_vorigeX = 0, m_vorigeY = 0;
    qint64 m_volgendeHerhaling = 0;
};
