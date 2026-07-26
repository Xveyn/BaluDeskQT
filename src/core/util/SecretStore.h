#pragma once

#include <optional>
#include <string>

namespace baludesk::core {

// Die plattformunabhängige Naht für die Ablage von Geheimnissen im
// Anmeldeinformationsspeicher des Betriebssystems.
//
// CredentialStore (core/util/CredentialStore.h) ist eine rein statische
// Klasse ohne Schnittstelle - ihre Tests schreiben deshalb zwangsläufig in
// den echten Windows Credential Manager. Stufe 1b braucht dichte Tests für
// Sitzungslogik (Token beim Start wiederherstellen, beim Abmelden löschen,
// fehlenden Refresh-Token behandeln); genau dafür ist diese Schnittstelle
// gedacht, gegen die sich ein Fake einsetzen lässt, ohne echten OS-Zustand
// zu verändern.
//
// Realisierung: WindowsCredentialStore (core/util/WindowsCredentialStore.h).
// Die Linux- und macOS-Realisierungen folgen, wenn diese Plattformen an der
// Reihe sind.
//
// Alle Zeichenketten sind UTF-8, in Schlüsseln wie in Werten. Werte dürfen
// Nullbytes enthalten.
class SecretStore {
public:
    virtual ~SecretStore() = default;

    // Legt das Geheimnis ab und überschreibt einen vorhandenen Eintrag.
    // Ein leerer Schlüssel wird abgelehnt.
    virtual bool save(const std::string& key, const std::string& secret) = 0;

    // Liest das Geheimnis. Nichts, wenn kein Eintrag existiert.
    [[nodiscard]] virtual std::optional<std::string> load(const std::string& key) = 0;

    // Entfernt den Eintrag. Ein nicht vorhandener Eintrag gilt als Erfolg -
    // das Ziel ist erreicht.
    virtual bool erase(const std::string& key) = 0;

    [[nodiscard]] virtual bool contains(const std::string& key) = 0;
};

}  // namespace baludesk::core
