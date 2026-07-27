#pragma once

#include <optional>
#include <string>

#include "core/auth/Session.h"

namespace baludesk::core {

// Ablage der Sitzung, austauschbar gemacht.
//
// Die Naht existiert aus demselben Grund wie Transport: damit die Logik
// darüber gegen einen Fake geprüft werden kann, statt bei jedem Testlauf
// echte Zugangsdaten des Benutzers anzufassen.
class SessionRepository {
public:
    virtual ~SessionRepository() = default;

    // Nichts, wenn keine vollständige Sitzung vorliegt. Eine halbe Sitzung -
    // Datei ohne Geheimnisse oder umgekehrt - gilt als keine.
    [[nodiscard]] virtual std::optional<Session> load() = 0;

    // Legt alle Bestandteile ab. Schlägt fehl, wenn auch nur einer nicht
    // gespeichert werden konnte; ein halb abgelegter Zustand wäre schlimmer
    // als gar keiner, weil der Server die Token kein zweites Mal ausliefert.
    virtual bool save(const Session& session) = 0;

    // Aktualisiert allein das Zugangstoken einer bereits abgelegten Sitzung.
    //
    // Getrennt von save(), weil beim Erneuern ausschliesslich dieser eine Wert
    // wechselt: Adresse, Benutzername und Pin - der gesamte Inhalt der Datei -
    // bleiben unberührt. Der Erneuerungspfad fasst die Datei damit gar nicht
    // an und kennt das Zeitfenster nicht, in dem neue Geheimnisse neben alten
    // Metadaten stehen könnten. Genau dieser Pfad wird ab Stufe 2 ständig
    // gefahren.
    //
    // Ein leeres Token wird abgelehnt: es würde eine funktionierende Sitzung
    // gegen eine kaputte tauschen.
    virtual bool saveAccessToken(const std::string& accessToken) = 0;

    // Entfernt alle Bestandteile. Auf einer leeren Ablage unschädlich.
    //
    // false, wenn etwas zurückblieb. Der Abmelde-Knopf soll nicht „erledigt"
    // melden, während ein Token im Anmeldeinformationsspeicher steht.
    virtual bool clear() = 0;
};

}  // namespace baludesk::core
