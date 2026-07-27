#pragma once

#include <string>

#include "core/api/HttpClient.h"
#include "core/auth/Session.h"

namespace baludesk::core {

class SessionRepository;

// Was ein Aufruf über den Zustand der Sitzung aussagt - unabhängig davon, was
// der Server inhaltlich geantwortet hat.
enum class AuthOutcome {
    // Der Aufruf ist durch. Der Statuscode in der Antwort sagt den Rest; er
    // darf durchaus 403 oder 404 sein.
    Ok,
    // Wie Ok, aber das Zugangstoken war abgelaufen und wurde unterwegs
    // erneuert. Der Aufrufer muss nichts tun; die Unterscheidung ist für das
    // Protokoll und für Anzeigen da.
    Refreshed,
    // Die Erneuerung wurde abgelehnt. Kein weiterer Versuch hilft, nur
    // Neupaaren. Von Unreachable zu unterscheiden ist der ganze Punkt: sonst
    // verwirft der Client eine gute Sitzung, weil der Server gerade schläft.
    ReauthRequired,
    // Das Zertifikat der Gegenstelle passt nicht mehr. Spec Abschnitt 2.9:
    // harter Abbruch, keine Nachfrage im laufenden Betrieb.
    TrustBroken,
    // Netz weg, Zeitlimit, Serverfehler. Die Sitzung ist unberührt; später
    // erneut versuchen.
    Unreachable
};

struct AuthenticatedResult {
    HttpResult response;
    AuthOutcome outcome = AuthOutcome::Ok;

    // Es kam eine Antwort des Servers zurück, die sich auswerten lässt.
    [[nodiscard]] bool usable() const {
        return outcome == AuthOutcome::Ok || outcome == AuthOutcome::Refreshed;
    }
};

// Ein HttpClient, der die Sitzung mitführt und ein abgelaufenes Zugangstoken
// still erneuert.
//
// Die eine Regel, die diese Klasse trägt: **genau ein Erneuerungsversuch je
// Aufruf.** Antwortet der Server auch auf die Wiederholung mit 401, ist das
// Ergebnis ein durchgereichter 401 und keine zweite Erneuerung. Ohne diese
// Grenze dreht sich der Client im Kreis.
//
// Das Erneuerungstoken wird vom Server nicht rotiert; erneuert wird
// ausschliesslich das Zugangstoken, abgelegt über
// SessionRepository::saveAccessToken().
//
// Die Klasse besitzt weder den HttpClient noch das Repository; beide müssen
// sie überleben.
class AuthenticatedClient {
public:
    AuthenticatedClient(HttpClient& http, SessionRepository& repository, Session session);

    AuthenticatedResult get(const std::string& path);
    AuthenticatedResult postJson(const std::string& path, const std::string& jsonBody);

    // Der aktuelle Stand, einschliesslich eines unterwegs erneuerten
    // Zugangstokens.
    [[nodiscard]] const Session& session() const { return session_; }

private:
    AuthenticatedResult dispatch(HttpMethod method,
                                 const std::string& path,
                                 const std::string& jsonBody);

    HttpResult perform(HttpMethod method, const std::string& path, const std::string& jsonBody);

    // Ok, wenn ein neues Zugangstoken vorliegt; sonst der Grund.
    AuthOutcome refresh();

    HttpClient& http_;
    SessionRepository& repository_;
    Session session_;
};

}  // namespace baludesk::core
