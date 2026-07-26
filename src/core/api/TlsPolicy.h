#pragma once

#include <string>

namespace baludesk::core {

// Wie eine Verbindung geprüft wird. Siehe Spec Abschnitt 2.9.
struct TlsPolicy {
    enum class Mode {
        // Normalfall: Prüfung gegen den Zertifikatsspeicher des Systems.
        SystemTrust,
        // Der Nutzer hat dieses eine Zertifikat beim Pairing bestätigt.
        // Geprüft wird dann gegen den angehefteten öffentlichen Schlüssel.
        Pinned
    };

    Mode mode = Mode::SystemTrust;

    // Nur bei Mode::Pinned belegt, im Format "sha256//<base64>" -
    // siehe CertificateIdentity::publicKeyPin in core/util/TlsTrust.h.
    std::string publicKeyPin;
};

}  // namespace baludesk::core
