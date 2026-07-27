#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "core/auth/SessionRepository.h"

namespace baludesk::core::test {

// Sitzungsablage im Arbeitsspeicher. Gegenstück zu FakeSecretStore, aus
// demselben Grund: Tests über der Ablage sollen weder die echte Datei noch
// den Anmeldeinformationsspeicher des Benutzers anfassen.
class FakeSessionRepository : public SessionRepository {
public:
    std::optional<Session> load() override { return stored; }

    bool save(const Session& session) override {
        ++saveCallCount;
        if (failSave) {
            return false;
        }
        stored = session;
        return true;
    }

    bool saveAccessToken(const std::string& accessToken) override {
        ++saveAccessTokenCallCount;
        if (failSaveAccessToken) {
            return false;
        }
        if (stored) {
            stored->accessToken = accessToken;
        }
        return true;
    }

    bool clear() override {
        ++clearCallCount;
        stored.reset();
        return !failClear;
    }

    std::optional<Session> stored;

    bool failSave = false;
    bool failSaveAccessToken = false;
    bool failClear = false;

    std::size_t saveCallCount = 0;
    std::size_t saveAccessTokenCallCount = 0;
    std::size_t clearCallCount = 0;
};

}  // namespace baludesk::core::test
