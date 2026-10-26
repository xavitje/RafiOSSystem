// RafiHub – gamepad.cpp

#include "gamepad.h"

#ifdef HEEFT_SDL2
#include <SDL.h>
#endif

namespace {
constexpr int STICK_DREMPEL = 16000;   // van de 32767: hoe ver de stick moet
constexpr qint64 EERSTE_HERHALING = 400; // ms voor een ingedrukte richting herhaalt
constexpr qint64 HERHALING = 130;        // ms tussen herhalingen
}

Gamepad::Gamepad(QObject *ouder) : QObject(ouder)
{
#ifdef HEEFT_SDL2
    // RafiHub heeft geen SDL-venster, dus SDL moet ook "op de achtergrond" luisteren.
    // (De interface reageert alleen als het RafiHub-venster actief is.)
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (SDL_Init(SDL_INIT_GAMECONTROLLER) != 0) {
        qWarning("rafihub: geen controller-ondersteuning: %s", SDL_GetError());
        return;
    }
    m_klok.start();
    connect(&m_timer, &QTimer::timeout, this, &Gamepad::controleer);
    m_timer.start(16); // ~60 keer per seconde
#endif
}

Gamepad::~Gamepad()
{
#ifdef HEEFT_SDL2
    if (m_pad) {
        SDL_GameControllerClose(static_cast<SDL_GameController *>(m_pad));
    }
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
#endif
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
#ifdef HEEFT_SDL2
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_CONTROLLERDEVICEADDED:
            if (!m_pad) {
                m_pad = SDL_GameControllerOpen(e.cdevice.which);
                if (m_pad) {
                    m_verbonden = true;
                    m_naam = QString::fromUtf8(
                        SDL_GameControllerName(static_cast<SDL_GameController *>(m_pad)));
                    emit verbondenVeranderd();
                }
            }
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            if (m_pad
                && e.cdevice.which
                    == SDL_JoystickInstanceID(
                        SDL_GameControllerGetJoystick(static_cast<SDL_GameController *>(m_pad)))) {
                SDL_GameControllerClose(static_cast<SDL_GameController *>(m_pad));
                m_pad = nullptr;
                m_verbonden = false;
                m_naam.clear();
                m_dx = m_dy = m_stickX = m_stickY = 0;
                emit verbondenVeranderd();
            }
            break;
        case SDL_CONTROLLERBUTTONDOWN:
            switch (e.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_A:
            case SDL_CONTROLLER_BUTTON_START: emit bevestig(); break;
            case SDL_CONTROLLER_BUTTON_B: emit terug(); break;
            case SDL_CONTROLLER_BUTTON_X: emit extra(); break;
            case SDL_CONTROLLER_BUTTON_Y: emit zoeken(); break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT: m_dx = -1; break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: m_dx = 1; break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP: m_dy = -1; break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN: m_dy = 1; break;
            default: break;
            }
            break;
        case SDL_CONTROLLERBUTTONUP:
            switch (e.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: m_dx = 0; break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN: m_dy = 0; break;
            default: break;
            }
            break;
        case SDL_CONTROLLERAXISMOTION: {
            const int w = e.caxis.value;
            const int stand = w > STICK_DREMPEL ? 1 : (w < -STICK_DREMPEL ? -1 : 0);
            if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
                m_stickX = stand;
            } else if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
                m_stickY = stand;
            }
            break;
        }
        default:
            break;
        }
    }
    // D-pad gaat voor de stick
    const int dx = m_dx != 0 ? m_dx : m_stickX;
    const int dy = m_dy != 0 ? m_dy : m_stickY;
    verwerkRichting(dx, dy);
#endif
}
