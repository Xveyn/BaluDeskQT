#pragma once

#include <optional>

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

    // Entfernt alle Bestandteile. Auf einer leeren Ablage unschädlich.
    virtual void clear() = 0;
};

}  // namespace baludesk::core
