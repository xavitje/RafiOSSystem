// RafiHub – gamepad.cpp
//
// De controller-bibliotheek SDL2 wordt pas bij het starten geladen (dlopen via
// QLibrary). Daardoor is er bij het bouwen geen SDL2-ontwikkelpakket nodig, en
// werkt RafiHub ook zonder SDL2 (dan alleen muis en toetsenbord).
//
// We gebruiken alleen functies zonder ingewikkelde structuren: de stand van de
// knoppen en sticks wordt elke 16 ms opgevraagd ("polling").

#include "gamepad.h"

#include <QLibrary>

namespace {

// Waarden uit SDL2 (SDL.h). Die liggen vast: SDL2 verandert ze nooit meer.
constexpr unsigned SDL_INIT_GAMECONTROLLER = 0x00002000u;
enum Knop {
    KNOP_A = 0,
    KNOP_B = 1,
    KNOP_X = 2,
    KNOP_Y = 3,
    KNOP_START = 6,
    KNOP_OMHOOG = 11,
    KNOP_OMLAAG = 12,
    KNOP_LINKS = 13,
    KNOP_RECHTS = 14,
};
constexpr int AS_LINKS_X = 0;
constexpr int AS_LINKS_Y = 1;

constexpr int STICK_DREMPEL = 16000;     // van de 32767: hoe ver de stick moet
constexpr qint64 EERSTE_HERHALING = 400; // ms voor een ingedrukte richting herhaalt
constexpr qint64 HERHALING = 130;        // ms tussen herhalingen

// De SDL2-functies die we nodig hebben
struct Sdl {
    int (*Init)(unsigned);
    void (*QuitSubSystem)(unsigned);
    int (*SetHint)(const char *, const char *);
    const char *(*GetError)();
    void (*PumpEvents)();
    void (*FlushEvents)(unsigned, unsigned);
    int (*NumJoysticks)();
    int (*IsGameController)(int);
    void *(*GameControllerOpen)(int);
    void (*GameControllerClose)(void *);
    int (*GameControllerGetAttached)(void *);
    unsigned char (*GameControllerGetButton)(void *, int);
    short (*GameControllerGetAxis)(void *, int);
    const char *(*GameControllerName)(void *);
};

Sdl sdl;
QLibrary *bibliotheek = nullptr;

bool laadSdl()
{
    bibliotheek = new QLibrary(QStringLiteral("SDL2-2.0"), 0);
    if (!bibliotheek->load()) {
        return false;
    }
    bool alles = true;
    auto pak = [&alles](auto &veld, const char *naam) {
        veld = reinterpret_cast<std::remove_reference_t<decltype(veld)>>(bibliotheek->resolve(naam));
        alles = alles && veld != nullptr;
    };
    pak(sdl.Init, "SDL_Init");
    pak(sdl.QuitSubSystem, "SDL_QuitSubSystem");
    pak(sdl.SetHint, "SDL_SetHint");
    pak(sdl.GetError, "SDL_GetError");
    pak(sdl.PumpEvents, "SDL_PumpEvents");
    pak(sdl.FlushEvents, "SDL_FlushEvents");
    pak(sdl.NumJoysticks, "SDL_NumJoysticks");
    pak(sdl.IsGameController, "SDL_IsGameController");
    pak(sdl.GameControllerOpen, "SDL_GameControllerOpen");
    pak(sdl.GameControllerClose, "SDL_GameControllerClose");
    pak(sdl.GameControllerGetAttached, "SDL_GameControllerGetAttached");
    pak(sdl.GameControllerGetButton, "SDL_GameControllerGetButton");
    pak(sdl.GameControllerGetAxis, "SDL_GameControllerGetAxis");
    pak(sdl.GameControllerName, "SDL_GameControllerName");
    return alles;
}

int stand(short waarde)
{
    return waarde > STICK_DREMPEL ? 1 : (waarde < -STICK_DREMPEL ? -1 : 0);
}

} // namespace

Gamepad::Gamepad(QObject *ouder) : QObject(ouder)
{
    if (qEnvironmentVariableIsSet("RAFIHUB_GEEN_CONTROLLER") || !laadSdl()) {
        return; // geen SDL2: alleen muis en toetsenbord
    }
    // RafiHub heeft geen SDL-venster, dus SDL moet ook "op de achtergrond" luisteren.
    // (De interface reageert alleen als het RafiHub-venster actief is.)
    sdl.SetHint("SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS", "1");
    if (sdl.Init(SDL_INIT_GAMECONTROLLER) != 0) {
        qWarning("rafihub: geen controller-ondersteuning: %s", sdl.GetError());
        return;
    }
    m_actief = true;
    m_klok.start();
    connect(&m_timer, &QTimer::timeout, this, &Gamepad::controleer);
    m_timer.start(16); // ~60 keer per seconde
}

Gamepad::~Gamepad()
{
    if (!m_actief) {
        return;
    }
    if (m_pad) {
        sdl.GameControllerClose(m_pad);
    }
    sdl.QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}

void Gamepad::verwerkRichting(int dx, int dy)
{
    const qint64 nu = m_klok.elapsed();
    if (dx == 0 && dy == 0) {
        m_vorigeX = m_vorigeY = 0;
        return;
    }
    if (dx != m_vorigeX || dy != m_vorigeY) {
        // Nieuwe richting: meteen bewegen, herhalen pas na een korte pauze
        emit richting(dx, dy);
        m_volgendeHerhaling = nu + EERSTE_HERHALING;
    } else if (nu >= m_volgendeHerhaling) {
        emit richting(dx, dy);
        m_volgendeHerhaling = nu + HERHALING;
    }
    m_vorigeX = dx;
    m_vorigeY = dy;
}

void Gamepad::controleer()
{
    // SDL laten kijken of er controllers bij- of afgekomen zijn, en de
    // berichtenwachtrij leeggooien (we lezen de knoppen direct uit).
    sdl.PumpEvents();
    sdl.FlushEvents(0, 0xFFFF);

    if (m_pad && !sdl.GameControllerGetAttached(m_pad)) {
        sdl.GameControllerClose(m_pad);
        m_pad = nullptr;
        m_verbonden = false;
        m_naam.clear();
        emit verbondenVeranderd();
    }
    if (!m_pad) {
        for (int i = 0; i < sdl.NumJoysticks(); ++i) {
            if (sdl.IsGameController(i) && (m_pad = sdl.GameControllerOpen(i)) != nullptr) {
                m_verbonden = true;
                const char *naam = sdl.GameControllerName(m_pad);
                m_naam = naam ? QString::fromUtf8(naam) : QStringLiteral("Controller");
                m_knoppen = 0xFFFFFFFFu; // knoppen die al ingedrukt zijn niet meetellen
                emit verbondenVeranderd();
                break;
            }
        }
        if (!m_pad) {
            return;
        }
    }

    // Knoppen: alleen reageren op "net ingedrukt"
    quint32 nu = 0;
    for (int k : {KNOP_A, KNOP_B, KNOP_X, KNOP_Y, KNOP_START}) {
        if (sdl.GameControllerGetButton(m_pad, k)) {
            nu |= 1u << k;
        }
    }
    const quint32 nieuw = nu & ~m_knoppen;
    m_knoppen = nu;
    if (nieuw & ((1u << KNOP_A) | (1u << KNOP_START))) {
        emit bevestig();
    }
    if (nieuw & (1u << KNOP_B)) {
        emit terug();
    }
    if (nieuw & (1u << KNOP_X)) {
        emit extra();
    }
    if (nieuw & (1u << KNOP_Y)) {
        emit zoeken();
    }

    // Richting: d-pad gaat voor de linker stick
    int dx = sdl.GameControllerGetButton(m_pad, KNOP_RECHTS) - sdl.GameControllerGetButton(m_pad, KNOP_LINKS);
    int dy = sdl.GameControllerGetButton(m_pad, KNOP_OMLAAG) - sdl.GameControllerGetButton(m_pad, KNOP_OMHOOG);
    if (dx == 0 && dy == 0) {
        dx = stand(sdl.GameControllerGetAxis(m_pad, AS_LINKS_X));
        dy = stand(sdl.GameControllerGetAxis(m_pad, AS_LINKS_Y));
    }
    verwerkRichting(dx, dy);
}
