// RafiHub – gamemodel.h
// De lijst met games zoals de interface (QML) hem ziet, plus zoeken en filteren.

#pragma once

#include "startopdracht.h"

#include <QAbstractListModel>
#include <QSortFilterProxyModel>

struct GameRij {
    GameInfo info;
    QString cover;
    qint64 laatstGespeeld = 0;
};

class GameModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Rol {
        IdRol = Qt::UserRole + 1,
        TitelRol,
        BronRol,
        AppidRol,
        PadRol,
        CoverRol,
        MangohudRol,
        LaatstRol,
        LettersRol,
        KleurRol,
    };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &ouder = {}) const override;
    QVariant data(const QModelIndex &index, int rol) const override;
    QHash<int, QByteArray> roleNames() const override;

    void zet(QList<GameRij> rijen);
    const GameRij *zoek(const QString &id) const;
    void werkBij(const QString &id, bool mangohud, qint64 laatstGespeeld);

    // Voor tegels zonder plaatje: beginletters en een vaste kleur per titel
    static QString letters(const QString &titel);
    static QString kleur(const QString &titel);

private:
    QList<GameRij> m_rijen;
};

class GameFilter : public QSortFilterProxyModel {
    Q_OBJECT
    Q_PROPERTY(QString zoekTekst READ zoekTekst WRITE zetZoekTekst NOTIFY filterVeranderd)
    Q_PROPERTY(QString bron READ bron WRITE zetBron NOTIFY filterVeranderd)
    Q_PROPERTY(int aantal READ aantal NOTIFY aantalVeranderd)
public:
    explicit GameFilter(QObject *ouder = nullptr);

    QString zoekTekst() const { return m_zoek; }
    void zetZoekTekst(const QString &t);
    QString bron() const { return m_bron; }
    void zetBron(const QString &b); // "alles", "steam" of "eigen"
    int aantal() const { return rowCount(); }

    Q_INVOKABLE QVariantMap get(int rij) const;
    Q_INVOKABLE int rijVan(const QString &id) const;

signals:
    void filterVeranderd();
    void aantalVeranderd();

protected:
    bool filterAcceptsRow(int bronRij, const QModelIndex &ouder) const override;
    bool lessThan(const QModelIndex &links, const QModelIndex &rechts) const override;

private:
    void opnieuw();
    QString m_zoek;
    QString m_bron = QStringLiteral("alles");
};
