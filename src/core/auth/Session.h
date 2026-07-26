#pragma once

#include <string>

namespace baludesk::core {

// Eine bestehende Bindung an einen Server.
//
// Die Bestandteile liegen an zwei Orten: Adresse, Benutzername und der
// bestätigte Zertifikat-Pin sind keine Geheimnisse und stehen in einer
// JSON-Datei; die beiden Token gehören in den Anmeldeinformationsspeicher des
// Betriebssystems. Zusammengesetzt wird das vom SessionRepository.
struct Session {
    // Normalisiert, ohne abschließenden Schrägstrich - siehe checkServerUrl().
    std::string serverUrl;

    // Für die Anzeige „verbunden mit … als …".
    std::string username;

    // Leer, wenn das Zertifikat des Servers regulär geprüft werden kann.
    // Andernfalls der beim Paaren bestätigte Schlüssel-Pin.
    std::string publicKeyPin;

    std::string accessToken;
    std::string refreshToken;

    // Ohne Adresse und ohne beide Token ist nichts anzufangen. Der Pin darf
    // fehlen - bei einem gültigen Zertifikat gibt es keinen.
    [[nodiscard]] bool isValid() const {
        return !serverUrl.empty() && !accessToken.empty() && !refreshToken.empty();
    }
};

}  // namespace baludesk::core
