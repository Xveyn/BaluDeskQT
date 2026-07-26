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

    // Erzeugt eine Regel, die gegen den angehefteten Schlüssel prüft.
    //
    // Der Name ist der Zweck: dieser Wert darf ausschließlich aus einem
    // abgeschlossenen Bestätigungsvorgang stammen, in dem ein Mensch den
    // Fingerabdruck gesehen und angenommen hat. Wer ihn anderswoher nimmt,
    // umgeht die Entscheidung, um die es in Spec Abschnitt 2.9 geht.
    //
    // Ein leerer Pin ergibt bewusst wieder SystemTrust statt einer Regel, die
    // nichts prüft.
    [[nodiscard]] static TlsPolicy pinnedFromConfirmedCertificate(const std::string& publicKeyPin) {
        TlsPolicy policy;
        if (publicKeyPin.empty()) {
            return policy;
        }
        policy.mode = Mode::Pinned;
        policy.publicKeyPin = publicKeyPin;
        return policy;
    }
};

}  // namespace baludesk::core
