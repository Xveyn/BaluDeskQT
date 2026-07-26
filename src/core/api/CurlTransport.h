#pragma once

#include <string>

#include "core/api/Transport.h"

namespace baludesk::core {

// libcurl hinter der Transport-Naht.
//
// Nicht unit-getestet: was hier passieren kann, passiert im Netz. Geprüft
// wird diese Klasse durch die Abnahme gegen einen echten Server.
//
// Instanzen dürfen beliebig entstehen und vergehen - auch mehrere
// nebeneinander oder nacheinander im selben Programmlauf. curl_global_init
// läuft dank std::once_flag genau einmal, beim ersten Konstruieren, bevor
// curl irgendeinen Thread aufmacht. curl_global_cleanup wird bewusst nicht
// aufgerufen: curls Vertrag verlangt, dass beim Aufräumen keine andere
// Instanz mehr aktiv ist, und das lässt sich hier nicht sicherstellen. Der
// globale Zustand von curl bleibt deshalb bis zum Prozessende bestehen - ein
// harmloses, von curl selbst so vorgesehenes Leck, der sichere Tausch gegen
// ein Aufräumen zur Unzeit.
class CurlTransport : public Transport {
public:
    CurlTransport();
    ~CurlTransport() override;

    CurlTransport(const CurlTransport&) = delete;
    CurlTransport& operator=(const CurlTransport&) = delete;

    TransportResult send(const std::string& url,
                         const HttpRequest& request,
                         const TlsPolicy& policy) override;
};

}  // namespace baludesk::core
