// RafiHub – vdf.cpp

#include "vdf.h"

#include <QFile>

namespace vdf {

namespace {

enum class Soort { Tekst, Open, Dicht, Einde };

struct Token {
    Soort soort;
    QString tekst;
    int regel;
};

// Knipt de tekst in stukjes: "tekst", {, } en het einde.
class Lezer {
public:
    explicit Lezer(const QString &tekst) : m_tekst(tekst) {}

    Token volgende()
    {
        slaOver();
        if (m_pos >= m_tekst.size()) {
            return {Soort::Einde, {}, m_regel};
        }
        const QChar c = m_tekst.at(m_pos);
        if (c == '{') {
            ++m_pos;
            return {Soort::Open, {}, m_regel};
        }
        if (c == '}') {
            ++m_pos;
            return {Soort::Dicht, {}, m_regel};
        }
        if (c == '"') {
            return tussenAanhalingstekens();
        }
        return zonderAanhalingstekens();
    }

    QString fout;

private:
    void slaOver()
    {
        while (m_pos < m_tekst.size()) {
            const QChar c = m_tekst.at(m_pos);
            if (c == '\n') {
                ++m_regel;
                ++m_pos;
            } else if (c.isSpace()) {
                ++m_pos;
            } else if (c == '/' && m_pos + 1 < m_tekst.size() && m_tekst.at(m_pos + 1) == '/') {
                while (m_pos < m_tekst.size() && m_tekst.at(m_pos) != '\n') {
                    ++m_pos; // commentaar tot het einde van de regel
                }
            } else {
                break;
            }
        }
    }

    Token tussenAanhalingstekens()
    {
        const int begin = m_regel;
        ++m_pos; // openend "
        QString uit;
        while (m_pos < m_tekst.size()) {
            const QChar c = m_tekst.at(m_pos++);
            if (c == '"') {
                return {Soort::Tekst, uit, begin};
            }
            if (c == '\n') {
                ++m_regel;
            }
            if (c == '\\' && m_pos < m_tekst.size()) {
                const QChar e = m_tekst.at(m_pos++);
                switch (e.unicode()) {
                case 'n': uit += '\n'; break;
                case 't': uit += '\t'; break;
                case '\\': uit += '\\'; break;
                case '"': uit += '"'; break;
                default: uit += '\\'; uit += e; break; // onbekend: laten staan
                }
                continue;
            }
            uit += c;
        }
        fout = QStringLiteral("regel %1: tekst zonder afsluitend \"").arg(begin);
        return {Soort::Einde, {}, begin};
    }

    Token zonderAanhalingstekens()
    {
        const int begin = m_pos;
        while (m_pos < m_tekst.size()) {
            const QChar c = m_tekst.at(m_pos);
            if (c.isSpace() || c == '{' || c == '}' || c == '"') {
                break;
            }
            ++m_pos;
        }
        return {Soort::Tekst, m_tekst.mid(begin, m_pos - begin), m_regel};
    }

    const QString &m_tekst;
    int m_pos = 0;
    int m_regel = 1;
};

// Leest "sleutel waarde"-paren tot een } (of het einde, op het hoogste niveau).
bool leesBlok(Lezer &lezer, QVariantMap &uit, bool hoogste, QString &fout, int diepte)
{
    if (diepte > 64) {
        fout = QStringLiteral("te diep genest");
        return false;
    }
    for (;;) {
        Token sleutel = lezer.volgende();
        if (!lezer.fout.isEmpty()) {
            fout = lezer.fout;
            return false;
        }
        if (sleutel.soort == Soort::Einde) {
            if (!hoogste) {
                fout = QStringLiteral("bestand eindigt terwijl er nog een { open staat");
                return false;
            }
            return true;
        }
        if (sleutel.soort == Soort::Dicht) {
            if (hoogste) {
                fout = QStringLiteral("regel %1: } zonder bijbehorende {").arg(sleutel.regel);
                return false;
            }
            return true;
        }
        if (sleutel.soort == Soort::Open) {
            fout = QStringLiteral("regel %1: { zonder naam ervoor").arg(sleutel.regel);
            return false;
        }

        Token waarde = lezer.volgende();
        if (!lezer.fout.isEmpty()) {
            fout = lezer.fout;
            return false;
        }
        // Voorwaarden zoals [$WIN32] na een sleutel overslaan
        while (waarde.soort == Soort::Tekst && waarde.tekst.startsWith('[')
               && waarde.tekst.endsWith(']')) {
            waarde = lezer.volgende();
        }

        const QString naam = sleutel.tekst.toLower();
        if (waarde.soort == Soort::Open) {
            QVariantMap kind;
            if (!leesBlok(lezer, kind, false, fout, diepte + 1)) {
                return false;
            }
            uit.insert(naam, kind);
        } else if (waarde.soort == Soort::Tekst) {
            uit.insert(naam, waarde.tekst);
        } else {
            fout = QStringLiteral("regel %1: waarde ontbreekt na \"%2\"")
                       .arg(sleutel.regel)
                       .arg(sleutel.tekst);
            return false;
        }
    }
}

} // namespace

Resultaat lees(const QString &tekst)
{
    Resultaat r;
    Lezer lezer(tekst);
    if (!leesBlok(lezer, r.data, true, r.fout, 0)) {
        r.data.clear();
    }
    return r;
}

Resultaat leesBestand(const QString &pad)
{
    QFile f(pad);
    if (!f.open(QIODevice::ReadOnly)) {
        return {{}, QStringLiteral("kan %1 niet openen").arg(pad)};
    }
    Resultaat r = lees(QString::fromUtf8(f.readAll()));
    if (!r.ok()) {
        r.fout = pad + QStringLiteral(": ") + r.fout;
    }
    return r;
}

QVariant zoek(const QVariantMap &data, const QStringList &pad)
{
    QVariant huidig = data;
    for (const QString &stap : pad) {
        const QVariantMap map = huidig.toMap();
        auto it = map.constFind(stap.toLower());
        if (it == map.constEnd()) {
            return {};
        }
        huidig = it.value();
    }
    return huidig;
}

} // namespace vdf
