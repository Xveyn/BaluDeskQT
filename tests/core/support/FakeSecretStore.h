#pragma once

#include <map>
#include <optional>
#include <string>

#include "core/util/SecretStore.h"

namespace baludesk::core::test {

// Geheimnisse im Arbeitsspeicher statt im Anmeldeinformationsspeicher des
// Betriebssystems. Damit lassen sich Sitzungstests fahren, ohne die echten
// Zugangsdaten des Benutzers anzufassen.
class FakeSecretStore : public SecretStore {
public:
    bool save(const std::string& key, const std::string& secret) override {
        if (key.empty() || failNextSave) {
            failNextSave = false;
            return false;
        }
        entries[key] = secret;
        return true;
    }

    [[nodiscard]] std::optional<std::string> load(const std::string& key) override {
        const auto it = entries.find(key);
        if (it == entries.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    bool erase(const std::string& key) override {
        entries.erase(key);
        return true;
    }

    [[nodiscard]] bool contains(const std::string& key) override {
        return entries.find(key) != entries.end();
    }

    std::map<std::string, std::string> entries;

    // Lässt den nächsten save()-Aufruf scheitern, um die Fehlerbehandlung zu
    // prüfen. Setzt sich danach selbst zurück.
    bool failNextSave = false;
};

}  // namespace baludesk::core::test
