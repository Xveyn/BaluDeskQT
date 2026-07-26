#include "core/api/CurlTransport.h"

#include <curl/curl.h>

#include <memory>
#include <mutex>
#include <string>

#include "core/util/Logger.h"

namespace baludesk::core {
namespace {

constexpr long kConnectTimeoutSeconds = 10;
constexpr long kRequestTimeoutSeconds = 30;

// Diese Schicht spricht eine JSON-API, keine Dateiübertragung - die bekommt
// in einer späteren Stufe einen eigenen Pfad. 8 MiB sind für eine JSON-Antwort
// großzügig bemessen und verhindern trotzdem, dass eine fehlerhafte oder
// böswillige Gegenstelle den Rumpf unbegrenzt wachsen lässt.
constexpr long kMaxResponseBytes = 8L * 1024L * 1024L;

std::size_t appendToString(char* data, std::size_t size, std::size_t count, void* userData) {
    auto* target = static_cast<std::string*>(userData);
    const std::size_t total = size * count;
    try {
        target->append(data, total);
    } catch (...) {
        // std::string::append kann bad_alloc werfen. Eine C++-Ausnahme durch
        // libcurls C-Rahmen zu schleusen ist undefiniertes Verhalten - hier
        // abgefangen und stattdessen 0 zurückgegeben, was curl als
        // CURLE_WRITE_ERROR sauber abbrechen lässt.
        return 0;
    }
    return total;
}

struct CurlDeleter {
    void operator()(CURL* handle) const noexcept { curl_easy_cleanup(handle); }
};
struct SlistDeleter {
    void operator()(curl_slist* list) const noexcept { curl_slist_free_all(list); }
};

// Das Zertifikat der Gegenstelle aus CURLINFO_CERTINFO ziehen. Nur nötig,
// wenn die Prüfung fehlgeschlagen ist und der Nutzer gefragt werden soll.
std::string peerCertificatePem(CURL* handle) {
    curl_certinfo* info = nullptr;
    if (curl_easy_getinfo(handle, CURLINFO_CERTINFO, &info) != CURLE_OK || info == nullptr) {
        return {};
    }
    if (info->num_of_certs <= 0) {
        return {};
    }

    // Das erste Zertifikat der Kette ist das des Servers.
    for (curl_slist* entry = info->certinfo[0]; entry != nullptr; entry = entry->next) {
        const std::string field = entry->data;
        const std::string prefix = "Cert:";
        if (field.rfind(prefix, 0) == 0) {
            return field.substr(prefix.size());
        }
    }
    return {};
}

HttpErrorKind classify(CURLcode code) {
    switch (code) {
        case CURLE_OK:
            return HttpErrorKind::None;
        case CURLE_PEER_FAILED_VERIFICATION:
            return HttpErrorKind::CertificateUntrusted;
        case CURLE_SSL_PINNEDPUBKEYNOTMATCH:
            return HttpErrorKind::CertificatePinMismatch;
        case CURLE_OPERATION_TIMEDOUT:
            return HttpErrorKind::Timeout;
        case CURLE_COULDNT_RESOLVE_HOST:
        case CURLE_COULDNT_RESOLVE_PROXY:
        case CURLE_COULDNT_CONNECT:
        case CURLE_SEND_ERROR:
        case CURLE_RECV_ERROR:
        case CURLE_GOT_NOTHING:
            return HttpErrorKind::Network;
        // CURLE_SSL_CACERT_BADFILE fällt bewusst hierher statt zu
        // CertificateUntrusted: es ist ein lokaler Konfigurationsfehler - die
        // CA-Datei war nicht lesbar oder kaputt, bevor die Gegenstelle
        // überhaupt erreicht wurde. Es gibt deshalb kein Zertifikat der
        // Gegenstelle zum Nachfragen, und CertificateUntrusted wäre hier
        // irreführend: es würde eine Nachfrage nach einem Fingerabdruck
        // auslösen, den es gar nicht gibt. Es ist damit auch der einzige Weg,
        // wie eine angeheftete Sitzung ohne einen echten Vertrauensbruch bei
        // CertificateUntrusted landen könnte - dieser Fall schließt genau das aus.
        default:
            return HttpErrorKind::Protocol;
    }
}

}  // namespace

CurlTransport::CurlTransport() {
    // Genau einmal je Programmlauf, unabhängig davon, wie viele Instanzen
    // entstehen. curls Vertrag verlangt außerdem, dass dies geschieht, bevor
    // andere Threads existieren - std::call_once beim ersten Konstruieren
    // greift früh genug, solange die allererste Instanz vor QApplication
    // (oder einem sonstigen Thread-Start) angelegt wird.
    static std::once_flag globalInitFlag;
    std::call_once(globalInitFlag, [] {
        const CURLcode code = curl_global_init(CURL_GLOBAL_DEFAULT);
        if (code != CURLE_OK) {
            Logger::error("curl_global_init fehlgeschlagen: {}", curl_easy_strerror(code));
        }
    });
}

CurlTransport::~CurlTransport() = default;

TransportResult CurlTransport::send(const std::string& url,
                                    const HttpRequest& request,
                                    const TlsPolicy& policy) {
    TransportResult result;

    std::unique_ptr<CURL, CurlDeleter> handle(curl_easy_init());
    if (!handle) {
        result.error = HttpErrorKind::Network;
        result.detail = "curl_easy_init fehlgeschlagen";
        return result;
    }

    std::string responseBody;
    char errorBuffer[CURL_ERROR_SIZE] = {};

    curl_easy_setopt(handle.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle.get(), CURLOPT_ERRORBUFFER, errorBuffer);
    curl_easy_setopt(handle.get(), CURLOPT_WRITEFUNCTION, appendToString);
    curl_easy_setopt(handle.get(), CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(handle.get(), CURLOPT_CONNECTTIMEOUT, kConnectTimeoutSeconds);
    curl_easy_setopt(handle.get(), CURLOPT_TIMEOUT, kRequestTimeoutSeconds);
    curl_easy_setopt(handle.get(), CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(handle.get(), CURLOPT_USERAGENT, "BaluDesk/0.1.0");
    curl_easy_setopt(handle.get(), CURLOPT_NOSIGNAL, 1L);
    // Obergrenze für den Antwortrumpf einer JSON-API - siehe kMaxResponseBytes.
    curl_easy_setopt(handle.get(), CURLOPT_MAXFILESIZE, kMaxResponseBytes);
    // Immer anfordern: bei einem Zertifikatsfehler ist das die einzige Quelle
    // für den Fingerabdruck, den der Nutzer bestätigen soll.
    curl_easy_setopt(handle.get(), CURLOPT_CERTINFO, 1L);

    // TLS. curl ist gegen OpenSSL gebaut; CURLSSLOPT_NATIVE_CA lässt trotzdem
    // den Windows-Wurzelspeicher gelten, damit ein Let's-Encrypt-Zertifikat
    // ohne mitgelieferte CA-Sammlung anerkannt wird.
    curl_easy_setopt(handle.get(), CURLOPT_SSL_OPTIONS, static_cast<long>(CURLSSLOPT_NATIVE_CA));
    // Mindestens TLS 1.2 erzwingen - ohne das erlaubt curl je nach
    // OpenSSL-Sicherheitsstufe der Zielmaschine auch TLS 1.0/1.1, was dem
    // Anspruch auf strikte Transportsicherheit widerspricht.
    curl_easy_setopt(handle.get(), CURLOPT_SSLVERSION, static_cast<long>(CURL_SSLVERSION_TLSv1_2));

    if (policy.mode == TlsPolicy::Mode::Pinned && !policy.publicKeyPin.empty()) {
        // Der Nutzer hat dieses Zertifikat bestätigt. Die Kettenprüfung kann
        // nicht gelingen - sonst wäre es nie zur Nachfrage gekommen -, also
        // wird sie abgeschaltet und durch den Pin ersetzt. curl prüft den Pin
        // beim Handschlag, bevor irgendein Byte des Rumpfes gesendet wird.
        curl_easy_setopt(handle.get(), CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(handle.get(), CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(handle.get(), CURLOPT_PINNEDPUBLICKEY, policy.publicKeyPin.c_str());
    } else {
        curl_easy_setopt(handle.get(), CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(handle.get(), CURLOPT_SSL_VERIFYHOST, 2L);
    }

    std::unique_ptr<curl_slist, SlistDeleter> headers;
    curl_slist* rawHeaders = nullptr;
    rawHeaders = curl_slist_append(rawHeaders, "Accept: application/json");
    if (request.method == HttpMethod::Post) {
        rawHeaders = curl_slist_append(rawHeaders, "Content-Type: application/json");
    }
    std::string authorization;
    if (!request.bearerToken.empty()) {
        authorization = "Authorization: Bearer " + request.bearerToken;
        rawHeaders = curl_slist_append(rawHeaders, authorization.c_str());
    }
    headers.reset(rawHeaders);
    curl_easy_setopt(handle.get(), CURLOPT_HTTPHEADER, headers.get());

    if (request.method == HttpMethod::Post) {
        curl_easy_setopt(handle.get(), CURLOPT_POST, 1L);
        curl_easy_setopt(handle.get(), CURLOPT_POSTFIELDS, request.jsonBody.c_str());
        curl_easy_setopt(handle.get(), CURLOPT_POSTFIELDSIZE,
                         static_cast<long>(request.jsonBody.size()));
    }

    const CURLcode code = curl_easy_perform(handle.get());

    result.error = classify(code);
    if (code != CURLE_OK) {
        result.detail = errorBuffer[0] != '\0' ? errorBuffer : curl_easy_strerror(code);
        if (result.error == HttpErrorKind::CertificateUntrusted) {
            result.peerCertificatePem = peerCertificatePem(handle.get());
        }
        return result;
    }

    long statusCode = 0;
    curl_easy_getinfo(handle.get(), CURLINFO_RESPONSE_CODE, &statusCode);
    result.response.statusCode = statusCode;
    result.response.body = std::move(responseBody);
    return result;
}

}  // namespace baludesk::core
