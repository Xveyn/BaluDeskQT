#include "core/auth/PairingClient.h"

#include <nlohmann/json.hpp>

#include <utility>

#include "core/api/HttpClient.h"
#include "core/util/Logger.h"

namespace baludesk::core {
namespace {

constexpr const char* kDeviceCodePath = "/api/desktop-pairing/device-code";
constexpr const char* kPollPath = "/api/desktop-pairing/poll";

// Wenn der Server bei 429 keinen Wert nennt, lieber deutlich zu lange warten
// als sich erneut auszusperren.
constexpr int kFallbackBackoffSeconds = 60;

// Ein Netzwerkfehler ist kein Grund aufzugeben - der Code lebt zehn Minuten,
// und ein NAS kann in der Zeit durchaus neu starten.
constexpr int kNetworkRetrySeconds = 10;

std::optional<nlohmann::json> parse(const std::string& body) {
    auto document = nlohmann::json::parse(body, nullptr, false);
    if (document.is_discarded() || !document.is_object()) {
        return std::nullopt;
    }
    return document;
}

std::string stringField(const nlohmann::json& document, const char* name) {
    const auto it = document.find(name);
    if (it == document.end() || !it->is_string()) {
        return {};
    }
    return it->get<std::string>();
}

int intField(const nlohmann::json& document, const char* name, int fallback) {
    const auto it = document.find(name);
    if (it == document.end() || !it->is_number_integer()) {
        return fallback;
    }
    return it->get<int>();
}

}  // namespace

PairingClient::PairingClient(HttpClient& http) : http_(http) {}

PairingProgress PairingClient::finish(PairingState state, std::string message) {
    state_ = state;
    PairingProgress progress;
    progress.state = state;
    progress.waitSeconds = 0;
    progress.message = std::move(message);
    return progress;
}

PairingProgress PairingClient::waiting(int waitSeconds, std::string message) {
    state_ = PairingState::WaitingForApproval;
    PairingProgress progress;
    progress.state = PairingState::WaitingForApproval;
    progress.waitSeconds = waitSeconds;
    progress.message = std::move(message);
    return progress;
}

PairingProgress PairingClient::begin(const std::string& deviceId,
                                     const std::string& deviceName,
                                     std::int64_t nowSeconds) {
    nlohmann::json request;
    request["device_id"] = deviceId;
    request["device_name"] = deviceName;
    // Der Server prüft gegen ^(windows|mac|linux)$. Stufe 1a wird nur unter
    // Windows gebaut; wenn weitere Plattformen dazukommen, gehört hier eine
    // Fallunterscheidung hin.
    request["platform"] = "windows";

    const HttpResult result = http_.postJson(kDeviceCodePath, request.dump());

    if (result.error == HttpErrorKind::CertificatePinMismatch) {
        Logger::error("Pairing: Zertifikat weicht vom angehefteten Schlüssel ab");
        return finish(PairingState::Failed,
                      "Das Zertifikat des Servers weicht vom bestätigten ab. "
                      "Die Verbindung wurde abgebrochen.");
    }
    if (!result.ok()) {
        Logger::warn("Pairing: Gerätecode nicht erhalten, Status {} — {}",
                     result.statusCode, result.detail);
        return finish(PairingState::Failed,
                      "Der Server hat keinen Gerätecode ausgestellt.");
    }

    const auto document = parse(result.body);
    if (!document) {
        Logger::error("Pairing: Antwort auf die Codeanforderung war kein JSON-Objekt");
        return finish(PairingState::Failed, "Die Antwort des Servers war unverständlich.");
    }

    const std::string deviceCode = stringField(*document, "device_code");
    const std::string userCode = stringField(*document, "user_code");
    if (deviceCode.empty() || userCode.empty()) {
        Logger::error("Pairing: Antwort ohne device_code oder user_code");
        return finish(PairingState::Failed, "Die Antwort des Servers war unvollständig.");
    }

    deviceCode_ = deviceCode;
    intervalSeconds_ = intField(*document, "interval", 5);
    if (intervalSeconds_ < 1) {
        intervalSeconds_ = 5;
    }

    PairingPrompt readyPrompt;
    readyPrompt.userCode = userCode;
    readyPrompt.verificationUrl = stringField(*document, "verification_url");
    readyPrompt.expiresInSeconds = intField(*document, "expires_in", 600);
    prompt_ = readyPrompt;

    deadlineSeconds_ = nowSeconds + readyPrompt.expiresInSeconds;

    Logger::info("Pairing: Code ausgestellt, gültig {} s, Intervall {} s",
                 readyPrompt.expiresInSeconds, intervalSeconds_);

    return waiting(intervalSeconds_);
}

PairingProgress PairingClient::poll(std::int64_t nowSeconds) {
    if (state_ == PairingState::Idle) {
        PairingProgress progress;
        progress.state = PairingState::Idle;
        progress.message = "Es läuft keine Paarung.";
        return progress;
    }
    if (state_ != PairingState::WaitingForApproval) {
        // Endzustand. Nicht erneut fragen - nach der Freigabe hat der Server
        // seinen Datensatz gelöscht, ein weiterer Abruf brächte nur 404.
        PairingProgress progress;
        progress.state = state_;
        return progress;
    }

    if (nowSeconds >= deadlineSeconds_) {
        Logger::info("Pairing: Gültigkeit des Codes abgelaufen");
        return finish(PairingState::Expired,
                      "Der Code ist abgelaufen. Bitte die Paarung neu starten.");
    }

    nlohmann::json request;
    request["device_code"] = deviceCode_;

    const HttpResult result = http_.postJson(kPollPath, request.dump());

    if (result.error == HttpErrorKind::CertificatePinMismatch) {
        Logger::error("Pairing: Zertifikat weicht vom angehefteten Schlüssel ab");
        return finish(PairingState::Failed,
                      "Das Zertifikat des Servers weicht vom bestätigten ab. "
                      "Die Verbindung wurde abgebrochen.");
    }
    if (result.error != HttpErrorKind::None) {
        // Netz weg, Zeitlimit, Zertifikat noch nicht bestätigt: alles
        // vorübergehend. Später erneut versuchen, solange der Code lebt.
        Logger::debug("Pairing: Abruf fehlgeschlagen ({}), neuer Versuch in {} s",
                      result.detail, kNetworkRetrySeconds);
        return waiting(kNetworkRetrySeconds);
    }

    if (result.statusCode == 429) {
        int retryAfter = kFallbackBackoffSeconds;
        if (const auto document = parse(result.body)) {
            retryAfter = intField(*document, "retry_after", kFallbackBackoffSeconds);
        }
        if (retryAfter < 1) {
            retryAfter = kFallbackBackoffSeconds;
        }
        Logger::warn("Pairing: Ratenbegrenzung erreicht, warte {} s", retryAfter);
        return waiting(retryAfter, "Der Server bremst die Abfragen. Es geht gleich weiter.");
    }

    if (result.statusCode == 404) {
        // "Invalid device code" - der Datensatz existiert nicht mehr.
        Logger::info("Pairing: Server kennt den Gerätecode nicht mehr");
        return finish(PairingState::Expired,
                      "Der Code ist nicht mehr gültig. Bitte die Paarung neu starten.");
    }

    if (!result.ok()) {
        Logger::warn("Pairing: unerwarteter Status {} beim Abruf", result.statusCode);
        return waiting(intervalSeconds_);
    }

    const auto document = parse(result.body);
    if (!document) {
        Logger::error("Pairing: Abrufantwort war kein JSON-Objekt");
        return finish(PairingState::Failed, "Die Antwort des Servers war unverständlich.");
    }

    const std::string status = stringField(*document, "status");

    if (status == "authorization_pending") {
        return waiting(intervalSeconds_);
    }
    if (status == "denied") {
        Logger::info("Pairing: im Browser abgelehnt");
        return finish(PairingState::Denied, "Die Paarung wurde abgelehnt.");
    }
    if (status == "expired") {
        Logger::info("Pairing: vom Server als abgelaufen gemeldet");
        return finish(PairingState::Expired,
                      "Der Code ist abgelaufen. Bitte die Paarung neu starten.");
    }
    if (status == "approved") {
        PairingTokens received;
        received.accessToken = stringField(*document, "access_token");
        received.refreshToken = stringField(*document, "refresh_token");

        const auto user = document->find("user");
        if (user != document->end() && user->is_object()) {
            received.username = stringField(*user, "username");
        }

        if (received.accessToken.empty() || received.refreshToken.empty()) {
            // Freigegeben, aber ohne Token. Der Server hat seinen Datensatz
            // dabei gelöscht - ein zweiter Versuch hilft nicht.
            Logger::error("Pairing: Freigabe ohne verwertbare Token erhalten");
            return finish(PairingState::Failed,
                          "Der Server hat die Paarung freigegeben, aber keine Zugangsdaten "
                          "geliefert. Bitte die Paarung neu starten.");
        }

        tokens_ = received;
        Logger::info("Pairing: freigegeben für Benutzer {}", received.username);
        return finish(PairingState::Approved, {});
    }

    Logger::warn("Pairing: unbekannter Status „{}“", status);
    return waiting(intervalSeconds_);
}

}  // namespace baludesk::core
