#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "core/api/HttpTypes.h"
#include "core/api/TlsPolicy.h"
#include "core/api/Transport.h"

namespace baludesk::core {

struct HttpResult {
    HttpErrorKind error = HttpErrorKind::None;
    long statusCode = 0;
    std::string body;
    std::string detail;
    std::string peerCertificatePem;

    // Erfolgreich heißt: kein Transportfehler und ein 2xx-Status.
    [[nodiscard]] bool ok() const {
        return error == HttpErrorKind::None && statusCode >= 200 && statusCode < 300;
    }
};

// Setzt Anfragen gegen einen Server zusammen und deutet die Antworten.
// Kennt weder curl noch Qt.
//
// Aus mehreren Threads gefahrlos aufrufbar: get()/postJson() und
// setTlsPolicy() dürfen gleichzeitig laufen, etwa wenn eine Abfrageschleife
// abseits des GUI-Threads läuft, während der GUI-Thread nach Bestätigung
// eines Zertifikats setTlsPolicy() aufruft. Nur die TLS-Regel selbst ist
// dafür synchronisiert - die Klasse als Ganzes ist es nicht.
class HttpClient {
public:
    HttpClient(std::string baseUrl, std::shared_ptr<Transport> transport, TlsPolicy policy);

    HttpResult get(const std::string& path, const std::string& bearerToken = {});
    HttpResult postJson(const std::string& path,
                        const std::string& jsonBody,
                        const std::string& bearerToken = {});

    // Ohne abschließenden Schrägstrich.
    [[nodiscard]] const std::string& baseUrl() const { return baseUrl_; }

    // Wird gesetzt, sobald der Nutzer ein Zertifikat bestätigt hat.
    void setTlsPolicy(TlsPolicy policy);

private:
    HttpResult send(HttpRequest request);
    [[nodiscard]] std::string urlFor(const std::string& path) const;

    std::string baseUrl_;
    std::shared_ptr<Transport> transport_;

    std::mutex policyMutex_;
    TlsPolicy policy_;
};

}  // namespace baludesk::core
