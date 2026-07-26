#pragma once

#include <string>

#include "core/auth/SessionRepository.h"

namespace baludesk::core {

class SecretStore;

// Sitzung in einer JSON-Datei plus dem Anmeldeinformationsspeicher.
//
// Bewusst ohne QSettings: das Prüfprogramm muss dieselbe Ablage benutzen wie
// später die Oberfläche, sonst beweist die Abnahme nichts über den echten
// Betrieb - und es ist Qt-frei. Die Bedienvorlieben in AppConfig bleiben
// davon unberührt, die gehören der Oberfläche.
class FileSessionRepository : public SessionRepository {
public:
    FileSessionRepository(std::string jsonPath, SecretStore& secrets);

    [[nodiscard]] std::optional<Session> load() override;
    bool save(const Session& session) override;
    void clear() override;

private:
    std::string jsonPath_;
    SecretStore& secrets_;
};

}  // namespace baludesk::core
