#pragma once

#include <string>

namespace baludesk::core {

enum class HttpMethod { Get, Post };

struct HttpRequest {
    HttpMethod method = HttpMethod::Get;

    // Pfad ab der Basis-URL, mit oder ohne führenden Schrägstrich.
    std::string path;

    // Nur bei Post ausgewertet. Wird unverändert gesendet, mit
    // Content-Type: application/json.
    std::string jsonBody;

    // Leer lassen, wenn die Anfrage ohne Anmeldung geht.
    std::string bearerToken;
};

struct HttpResponse {
    long statusCode = 0;
    std::string body;
};

// Warum eine Anfrage gescheitert ist. Absichtlich grob: der Client muss
// unterscheiden können, was daraus folgt, nicht jeden curl-Code kennen.
enum class HttpErrorKind {
    None,
    // Nicht erreichbar, DNS, Verbindung abgewiesen, Verbindung abgerissen.
    Network,
    // Zertifikat nicht vertrauenswürdig - selbstsigniert, falscher Name,
    // abgelaufen. Führt zur Nachfrage beim Nutzer.
    CertificateUntrusted,
    // Angehefteter Schlüssel passt nicht. Führt NICHT zur Nachfrage, sondern
    // zum harten Abbruch (Spec Abschnitt 2.9).
    CertificatePinMismatch,
    Timeout,
    // Antwort unverständlich, HTTP-Ebene kaputt.
    Protocol
};

struct TransportResult {
    HttpErrorKind error = HttpErrorKind::None;
    HttpResponse response;

    // Klartext für Protokoll und Anzeige, etwa curls Fehlermeldung.
    std::string detail;

    // Zertifikat der Gegenstelle im PEM-Format, wenn es zu ermitteln war.
    // Wird bei CertificateUntrusted gebraucht, damit der Nutzer den
    // Fingerabdruck bestätigen kann.
    std::string peerCertificatePem;
};

}  // namespace baludesk::core
