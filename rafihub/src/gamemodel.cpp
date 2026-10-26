// RafiHub – gamemodel.cpp

#include "gamemodel.h"

#include <QColor>
#include <QUrl>

int GameModel::rowCount(const QModelIndex &ouder) const
{
    return ouder.isValid() ? 0 : static_cast<int>(m_rijen.size());
}

QVariant GameModel::data(const QModelIndex &index, int rol) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rijen.size()) {
        return {};
    }
    const GameRij &r = m_rijen.at(index.row());
    switch (rol) {
    case IdRol: return r.info.id;
    case Qt::DisplayRole:
    case TitelRol: return r.info.titel;
    case BronRol: return r.info.bron == Bron::Steam ? QStringLiteral("steam") : QStringLiteral("eigen");
    case AppidRol: return r.info.appid;
    case PadRol: return r.info.pad;
    case CoverRol: return r.cover.isEmpty() ? QString() : QUrl::fromLocalFile(r.cover).toString();
    case MangohudRol: return r.info.mangohud;
    case LaatstRol: return r.laatstGespeeld;
    case LettersRol: return letters(r.info.titel);
    case KleurRol: return kleur(r.info.titel);
    default: return {};
    }
}

QHash<int, QByteArray> GameModel::roleNames() const
{
    return {{IdRol, "gameId"},     {TitelRol, "titel"},       {BronRol, "bron"},
            {AppidRol, "appid"},   {PadRol, "pad"},           {CoverRol, "cover"},
            {MangohudRol, "mangohud"}, {LaatstRol, "laatstGespeeld"},
            {LettersRol, "letters"}, {KleurRol, "kleur"}};
}

void GameModel::zet(QList<GameRij> rijen)
{
    beginResetModel();
    m_rijen = std::move(rijen);
    endResetModel();
}

const GameRij *GameModel::zoek(const QString &id) const
{
    for (const GameRij &r : m_rijen) {
        if (r.info.id == id) {
            return &r;
        }
    }
    return nullptr;
}

void GameModel::werkBij(const QString &id, bool mangohud, qint64 laatstGespeeld)
{
    for (int i = 0; i < m_rijen.size(); ++i) {
        if (m_rijen[i].info.id == id) {
            m_rijen[i].info.mangohud = mangohud;
            m_rijen[i].laatstGespeeld = laatstGespeeld;
            emit dataChanged(index(i), index(i), {MangohudRol, LaatstRol});
            return;
        }
    }
}

QString GameModel::letters(const QString &titel)
{
    QString uit;
    const QStringList woorden = titel.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &w : woorden) {
        if (w.at(0).isLetterOrNumber()) {
            uit += w.at(0).toUpper();
        }
        if (uit.size() == 2) {
            break;
        }
    }
    return uit.isEmpty() ? QStringLiteral("?") : uit;
}

QString GameModel::kleur(const QString &titel)
{
    // Elke titel krijgt altijd dezelfde kleur, verdeeld over de hele kleurencirkel
    const uint h = qHash(titel) % 360;
    return QColor::fromHsv(static_cast<int>(h), 150, 190).name();
}

// ---- Filter ----

GameFilter::GameFilter(QObject *ouder) : QSortFilterProxyModel(ouder)
{
    setDynamicSortFilter(true);
    // Qt sorteert alleen opnieuw als de sorteer-rol verandert: dat is "laatst gespeeld"
    setSortRole(GameModel::LaatstRol);
    sort(0);
    connect(this, &QAbstractItemModel::rowsInserted, this, &GameFilter::aantalVeranderd);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &GameFilter::aantalVeranderd);
    connect(this, &QAbstractItemModel::modelReset, this, &GameFilter::aantalVeranderd);
    connect(this, &QAbstractItemModel::layoutChanged, this, &GameFilter::aantalVeranderd);
}

void GameFilter::zetZoekTekst(const QString &t)
{
    if (t != m_zoek) {
        m_zoek = t;
        opnieuw();
    }
}

void GameFilter::zetBron(const QString &b)
{
    if (b != m_bron) {
        m_bron = b;
        opnieuw();
    }
}

void GameFilter::opnieuw()
{
    invalidateFilter();
    emit filterVeranderd();
    emit aantalVeranderd();
}

bool GameFilter::filterAcceptsRow(int bronRij, const QModelIndex &ouder) const
{
    const QModelIndex i = sourceModel()->index(bronRij, 0, ouder);
    if (m_bron != QStringLiteral("alles") && i.data(GameModel::BronRol).toString() != m_bron) {
        return false;
    }
    return m_zoek.trimmed().isEmpty()
        || i.data(GameModel::TitelRol).toString().contains(m_zoek.trimmed(), Qt::CaseInsensitive);
}

bool GameFilter::lessThan(const QModelIndex &links, const QModelIndex &rechts) const
{
    // Laatst gespeeld eerst, daarna op alfabet
    const qint64 a = links.data(GameModel::LaatstRol).toLongLong();
    const qint64 b = rechts.data(GameModel::LaatstRol).toLongLong();
    if (a != b) {
        return a > b;
    }
    return links.data(GameModel::TitelRol).toString().localeAwareCompare(
               rechts.data(GameModel::TitelRol).toString())
        < 0;
}

QVariantMap GameFilter::get(int rij) const
{
    QVariantMap uit;
    if (rij < 0 || rij >= rowCount()) {
        return uit;
    }
    const QModelIndex i = index(rij, 0);
    const QHash<int, QByteArray> namen = sourceModel()->roleNames();
    for (auto it = namen.constBegin(); it != namen.constEnd(); ++it) {
        uit.insert(QString::fromUtf8(it.value()), i.data(it.key()));
    }
    return uit;
}

int GameFilter::rijVan(const QString &id) const
{
    for (int r = 0; r < rowCount(); ++r) {
        if (index(r, 0).data(GameModel::IdRol).toString() == id) {
            return r;
        }
    }
    return -1;
}
