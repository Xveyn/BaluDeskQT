#include "core/auth/AuthenticatedClient.h"

#include <nlohmann/json.hpp>

#include <utility>

#include "core/auth/SessionRepository.h"
#include "core/util/Logger.h"

namespace baludesk::core {
namespace {

constexpr const char* kRefreshPath = "/api/auth/refresh";

// Was ein Transportfehler für die Sitzung bedeutet.
AuthOutcome outcomeFor(HttpErrorKind error) {
    switch (error) {
        case HttpErrorKind::None:
            return AuthOutcome::Ok;
        case HttpErrorKind::CertificatePinMismatch:
        case HttpErrorKind::CertificateUntrusted:
            // Beide heissen: dieser Gegenstelle ist gerade nicht zu trauen.
            // Ob der angeheftete Schlüssel abweicht oder ein Zertifikat ohne
            // Pin abgelaufen ist, ändert nichts an der Folge - es wird nicht
            // weitergeredet und im laufenden Betrieb auch nicht nachgefragt.
            return AuthOutcome::TrustBroken;
        case HttpErrorKind::Network:
        case HttpErrorKind::Timeout:
        case HttpErrorKind::Protocol:
            return AuthOutcome::Unreachable;
    }
    return AuthOutcome::Unreachable;
}

std::string accessTokenFrom(const std::string& body) {
    auto document = nlohmann::json::parse(body, nullptr, false);
    if (document.is_discarded() || !document.is_object()) {
        return {};
    }
    const auto it = document.find("access_token");
    if (it == document.end() || !it->is_string()) {
        return {};
    }
    return it->get<std::string>();
}

}  // namespace

AuthenticatedClient::AuthenticatedClient(HttpClient& http,
                                         SessionRepository& repository,
                                         Session session)
    : http_(http), repository_(repository), session_(std::move(session)) {}

HttpResult AuthenticatedClient::perform(HttpMethod method,
                                        const std::string& path,
                                        const std::string& jsonBody) {
    if (method == HttpMethod::Post) {
        return http_.postJson(path, jsonBody, session_.accessToken);
    }
    return http_.get(path, session_.accessToken);
}

AuthOutcome AuthenticatedClient::refresh() {
    nlohmann::json request;
    request["refresh_token"] = session_.refreshToken;

    // Ohne Bearer: die Erneuerung weist sich über das Erneuerungstoken im
    // Rumpf aus, und das abgelaufene Zugangstoken mitzuschicken hätte
    // bestenfalls keine Wirkung.
    const HttpResult result = http_.postJson(kRefreshPath, request.dump());

    const AuthOutcome transport = outcomeFor(result.error);
    if (transport != AuthOutcome::Ok) {
        Logger::debug("Sitzung: Erneuerung nicht zustellbar — {}", result.detail);
        return transport;
    }

    if (result.statusCode == 401 || result.statusCode == 403) {
        Logger::warn("Sitzung: Erneuerungstoken abgelehnt (Status {})", result.statusCode);
        return AuthOutcome::ReauthRequired;
    }

    if (!result.ok()) {
        // 5xx und alles Übrige: ein Problem des Servers, keine Aussage über
        // das Token.
        Logger::warn("Sitzung: Erneuerung mit Status {} fehlgeschlagen", result.statusCode);
        return AuthOutcome::Unreachable;
    }

    const std::string token = accessTokenFrom(result.body);
    if (token.empty()) {
        Logger::error("Sitzung: Erneuerung ohne access_token beantwortet");
        return AuthOutcome::ReauthRequired;
    }

    session_.accessToken = token;

    if (!repository_.saveAccessToken(token)) {
        // Nicht abbrechen: der Aufruf, der gleich folgt, arbeitet mit dem
        // Token im Speicher und gelingt. Verloren ist allein die Haltbarkeit
        // über das Programmende hinaus - beim nächsten Start wird eben erneut
        // erneuert.
        Logger::error("Sitzung: erneuertes Zugangstoken konnte nicht abgelegt werden");
    }

    Logger::info("Sitzung: Zugangstoken erneuert");
    return AuthOutcome::Ok;
}

AuthenticatedResult AuthenticatedClient::dispatch(HttpMethod method,
                                                  const std::string& path,
                                                  const std::string& jsonBody) {
    AuthenticatedResult result;
    result.response = perform(method, path, jsonBody);
    result.outcome = outcomeFor(result.response.error);

    if (result.outcome != AuthOutcome::Ok) {
        return result;
    }
    if (result.response.statusCode != 401) {
        // Durch. Auch ein 403 oder 404 ist eine Antwort, keine Frage an die
        // Sitzung.
        return result;
    }

    const AuthOutcome refreshOutcome = refresh();
    if (refreshOutcome != AuthOutcome::Ok) {
        result.outcome = refreshOutcome;
        return result;
    }

    // Der eine erlaubte zweite Versuch.
    result.response = perform(method, path, jsonBody);
    result.outcome = outcomeFor(result.response.error);
    if (result.outcome == AuthOutcome::Ok && result.response.ok()) {
        result.outcome = AuthOutcome::Refreshed;
    }
    return result;
}

AuthenticatedResult AuthenticatedClient::get(const std::string& path) {
    return dispatch(HttpMethod::Get, path, {});
}

AuthenticatedResult AuthenticatedClient::postJson(const std::string& path,
                                                  const std::string& jsonBody) {
    return dispatch(HttpMethod::Post, path, jsonBody);
}

}  // namespace baludesk::core
