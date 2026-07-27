#include "core/api/TrustNegotiation.h"

#include <utility>

#include "core/api/HttpClient.h"
#include "core/api/TlsPolicy.h"
#include "core/util/Logger.h"
#include "core/util/TlsTrust.h"

namespace baludesk::core {
namespace {

// Der günstigste Endpunkt für einen Kontaktversuch: ohne Anmeldung, ohne
// Nebenwirkung, und er existiert auf jedem BaluHost (Spec Abschnitt 9).
constexpr const char* kHealthPath = "/api/health";

}  // namespace

TrustNegotiation::TrustNegotiation(HttpClient& http) : http_(http) {}

TrustState TrustNegotiation::settle(TrustState state, std::string message) {
    state_ = state;
    message_ = std::move(message);
    return state_;
}

TrustState TrustNegotiation::evaluate() {
    const HttpResult result = http_.get(kHealthPath);

    if (result.error == HttpErrorKind::CertificatePinMismatch) {
        Logger::error("Vertrauen: Zertifikat weicht vom angehefteten Schlüssel ab");
        prompt_.reset();
        return settle(TrustState::PinMismatch,
                      "Der Server weist ein anderes Zertifikat vor als das beim Verbinden "
                      "bestätigte.");
    }

    if (result.error == HttpErrorKind::CertificateUntrusted) {
        if (result.peerCertificatePem.empty()) {
            Logger::warn("Vertrauen: Zertifikat nicht vertrauenswürdig und nicht auszulesen");
            prompt_.reset();
            return settle(TrustState::Failed,
                          "Dem Zertifikat des Servers wird nicht vertraut, und es war keines "
                          "auszulesen, das sich vergleichen ließe.");
        }

        const auto identity = identifyCertificate(result.peerCertificatePem);
        if (!identity) {
            Logger::warn("Vertrauen: Zertifikat der Gegenstelle ist nicht lesbar");
            prompt_.reset();
            return settle(TrustState::Failed,
                          "Das Zertifikat des Servers ist nicht lesbar.");
        }

        TrustPrompt readyPrompt;
        readyPrompt.fingerprintSha256 = identity->fingerprintSha256;
        readyPrompt.publicKeyPin = identity->publicKeyPin;
        readyPrompt.reason = result.detail;
        prompt_ = readyPrompt;

        Logger::info("Vertrauen: unbekanntes Zertifikat, Bestätigung nötig — {}", result.detail);
        return settle(TrustState::ConfirmationRequired, {});
    }

    if (result.error == HttpErrorKind::Network || result.error == HttpErrorKind::Timeout) {
        Logger::debug("Vertrauen: Server nicht erreichbar — {}", result.detail);
        prompt_.reset();
        return settle(TrustState::Unreachable,
                      "Der Server ist nicht erreichbar: " + result.detail);
    }

    if (result.error != HttpErrorKind::None) {
        Logger::warn("Vertrauen: Verbindung unbrauchbar — {}", result.detail);
        prompt_.reset();
        return settle(TrustState::Failed,
                      "Die Verbindung zum Server ist unbrauchbar: " + result.detail);
    }

    if (!result.ok()) {
        // Die Verbindung steht, aber am anderen Ende antwortet kein BaluHost.
        Logger::warn("Vertrauen: unerwarteter Status {} auf {}", result.statusCode, kHealthPath);
        prompt_.reset();
        return settle(TrustState::Failed,
                      "Unter dieser Adresse antwortet kein BaluHost-Server (Status " +
                          std::to_string(result.statusCode) + ").");
    }

    prompt_.reset();
    return settle(TrustState::Trusted, {});
}

TrustState TrustNegotiation::probe() {
    return evaluate();
}

TrustState TrustNegotiation::confirm() {
    if (state_ != TrustState::ConfirmationRequired || !prompt_) {
        // Nichts zu bestätigen. Bewusst ohne Netzzugriff und ohne
        // Zustandswechsel - dieselbe Bauart wie PairingClient::poll() nach
        // einem Endzustand.
        return state_;
    }

    const std::string offeredPin = prompt_->publicKeyPin;
    http_.setTlsPolicy(TlsPolicy::pinnedFromConfirmedCertificate(offeredPin));

    const TrustState result = evaluate();

    if (result == TrustState::Trusted) {
        confirmedPin_ = offeredPin;
        Logger::info("Vertrauen: Zertifikat bestätigt und Schlüssel angeheftet");
        return result;
    }

    if (result == TrustState::ConfirmationRequired) {
        // Kann baulich nicht eintreten: mit angehefteter Regel meldet der
        // Transport eine Abweichung als CertificatePinMismatch, nie als
        // CertificateUntrusted (belegt in der Abnahme der Stufe 1a). Der
        // Zweig steht hier, damit ein künftiger Umbau des Transports nicht
        // still in einer Endlosschleife aus Nachfragen landet.
        http_.setTlsPolicy(TlsPolicy{});
        prompt_.reset();
        return settle(TrustState::Failed,
                      "Das Zertifikat ließ sich auch nach der Bestätigung nicht prüfen.");
    }

    // Nicht bestätigt: der Pin bleibt ungesetzt, und die Regel geht auf volle
    // Prüfung zurück. Das ist kein Verlust einer vorher gesetzten Regel:
    // confirm() ist ausschließlich nach ConfirmationRequired wirksam, und
    // dieser Zustand entsteht nur aus CertificateUntrusted - im gepinnten
    // Modus meldet der Transport eine Abweichung als CertificatePinMismatch,
    // nie als CertificateUntrusted (siehe oben). Der HttpClient stand vor
    // diesem Aufruf also immer auf SystemTrust. Bliebe die eben gesetzte,
    // nie bestätigte Regel stehen, wäre eine spätere Prüfung über dasselbe
    // Paar gegen einen Schlüssel gerichtet, dem nie jemand zugestimmt hat.
    http_.setTlsPolicy(TlsPolicy{});
    confirmedPin_.clear();
    return result;
}

}  // namespace baludesk::core
