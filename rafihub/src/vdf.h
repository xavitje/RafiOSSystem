// RafiHub – vdf.h
// Steam bewaart zijn gegevens in "VDF" (Valve KeyValues), een eigen tekstformaat:
//
//   "AppState"
//   {
//       "appid"   "730"
//       "name"    "Counter-Strike 2"
//   }
//
// Deze lezer maakt daar geneste QVariantMaps van. Sleutels worden kleine
// letters, want Steam is daar zelf niet consequent in ("InstallDir" vs "installdir").

#pragma once

#include <QString>
#include <QVariantMap>

namespace vdf {

struct Resultaat {
    QVariantMap data;
    QString fout; // leeg = gelukt
    bool ok() const { return fout.isEmpty(); }
};

Resultaat lees(const QString &tekst);
Resultaat leesBestand(const QString &pad);

// Hulpje: haalt een waarde op via een pad van sleutels, bijv. {"appstate", "name"}.
QVariant zoek(const QVariantMap &data, const QStringList &pad);

} // namespace vdf
