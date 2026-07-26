#pragma once

#include <string>

namespace baludesk::core {

// Was an einer eingegebenen Serveradresse nicht stimmt.
enum class ServerUrlProblem {
    None,
    // Nichts eingegeben.
    Empty,
    // Kein Rechnername erkennbar.
    Malformed,
    // Etwas anderes als http oder https.
    UnsupportedScheme,
    // Blankes http zu einem fremden Rechner. Über diese Verbindung geht ein
    // Zugangstoken - siehe Spec Abschnitt 2.9.
    InsecureScheme
};

struct ServerUrlCheck {
    ServerUrlProblem problem = ServerUrlProblem::None;

    // Bereinigte Fassung: Leerraum entfernt, fehlendes Schema auf https
    // ergänzt, abschliessende Schrägstriche entfernt. Nur belegt, wenn die
    // Prüfung bestanden wurde.
    std::string normalized;

    // Erklärung im Klartext, zur Anzeige geeignet. Leer, wenn alles stimmt.
    std::string message;

    [[nodiscard]] bool ok() const { return problem == ServerUrlProblem::None; }
};

// Prüft und normalisiert eine eingegebene Serveradresse.
//
// Absichtlich hier und nicht im Eingabedialog: die Regel gilt für jeden
// Aufrufer, und der nächste Einstiegspunkt würde eine Prüfung im Dialog
// umgehen.
//
// Fehlt das Schema, wird https ergänzt. Blankes http ist ausschliesslich zu
// localhost erlaubt, damit Entwicklung gegen eine lokale Dev-Instanz möglich
// bleibt.
[[nodiscard]] ServerUrlCheck checkServerUrl(const std::string& input);

}  // namespace baludesk::core
