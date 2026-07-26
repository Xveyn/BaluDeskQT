#pragma once

#include <optional>
#include <string>

namespace baludesk::core {

// Ablage für Geheimnisse im Anmeldeinformationsspeicher des Betriebssystems.
//
// Windows: Credential Manager (wincred). Die Linux- und macOS-Zweige folgen,
// wenn diese Plattformen an der Reihe sind; bis dahin sind die Aufrufe dort
// nicht übersetzbar und das Ziel wird ohnehin nicht gebaut.
//
// Der Schlüssel ist ein freier Bezeichner wie "server/access-token". Er wird
// mit einem festen Präfix versehen, damit BaluDesks Einträge im Credential
// Manager erkennbar sind und nicht mit fremden kollidieren.
//
// Alle Zeichenketten sind UTF-8, in Schlüsseln wie in Werten. Werte dürfen
// Nullbytes enthalten.
class CredentialStore {
public:
    // Legt das Geheimnis ab und überschreibt einen vorhandenen Eintrag.
    // Ein leerer Schlüssel wird abgelehnt.
    static bool save(const std::string& key, const std::string& secret);

    // Liest das Geheimnis. Nichts, wenn kein Eintrag existiert.
    [[nodiscard]] static std::optional<std::string> load(const std::string& key);

    // Entfernt den Eintrag. Ein nicht vorhandener Eintrag gilt als Erfolg -
    // das Ziel ist erreicht.
    static bool erase(const std::string& key);

    [[nodiscard]] static bool contains(const std::string& key);
};

}  // namespace baludesk::core
