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
        ++saveCallCount;
        if (key.empty() || saveCallCount == failOnSaveNumber) {
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

    // Lässt den n-ten save()-Aufruf scheitern (1 = der erste). 0 heißt: nie.
    // Ein Zähler statt eines Schlüsselnamens, weil die Schlüssel private
    // Konstanten von FileSessionRepository sind — ein Test soll sie nicht
    // kennen müssen, um einen Zweig zu erreichen.
    std::size_t failOnSaveNumber = 0;

    std::size_t saveCallCount = 0;
};

}  // namespace baludesk::core::test
