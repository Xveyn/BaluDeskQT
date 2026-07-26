#pragma once

#include <optional>
#include <string>

namespace baludesk::core {

// Die beiden Kennungen eines Serverzertifikats, die BaluDesk braucht.
struct CertificateIdentity {
    // SHA-256 über das DER-kodierte Zertifikat, als Hexpaare mit Doppelpunkt:
    // "F4:B6:19:0C:...". Das ist die Darstellung, die dem Nutzer beim
    // Bestätigen gezeigt wird - dieselbe, die auch Browser und die
    // openssl-Kommandozeile ausgeben, damit sie sich vergleichen lässt.
    std::string fingerprintSha256;

    // SHA-256 über den DER-kodierten SubjectPublicKeyInfo, base64-kodiert und
    // mit "sha256//" davor. Genau dieses Format erwartet libcurl bei
    // CURLOPT_PINNEDPUBLICKEY.
    //
    // Angeheftet wird der öffentliche Schlüssel und nicht das Zertifikat,
    // damit eine Erneuerung des Zertifikats mit gleichem Schlüssel den Pin
    // nicht bricht.
    std::string publicKeyPin;
};

// Liest ein Zertifikat im PEM-Format und berechnet beide Kennungen.
//
// Gibt nichts zurück, wenn der Text kein gültiges Zertifikat enthält - leer,
// abgeschnitten, kein PEM, kaputtes Base64. Wirft nie.
[[nodiscard]] std::optional<CertificateIdentity> identifyCertificate(const std::string& pem);

}  // namespace baludesk::core
