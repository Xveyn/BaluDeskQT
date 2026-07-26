#pragma once

#include <string>

#include "core/api/Transport.h"

namespace baludesk::core {

// libcurl hinter der Transport-Naht.
//
// Nicht unit-getestet: was hier passieren kann, passiert im Netz. Geprüft
// wird diese Klasse durch die Abnahme gegen einen echten Server.
//
// curl_global_init/cleanup werden vom Konstruktor und Destruktor gehalten.
// Es darf deshalb nur eine Instanz je Programmlauf geben.
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
