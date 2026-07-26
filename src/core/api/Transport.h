#pragma once

#include <string>

#include "core/api/HttpTypes.h"
#include "core/api/TlsPolicy.h"

namespace baludesk::core {

// Die Naht, an der die Netzwerkschicht austauschbar wird.
//
// Alles Prüfbare - Pfadzusammensetzung, Kopfzeilen, Auswertung der Antwort -
// liegt in HttpClient darüber und ist gegen einen Fake testbar. Darunter
// bleibt nur der eigentliche curl-Aufruf, der von Hand abgenommen wird.
class Transport {
public:
    virtual ~Transport() = default;

    // Führt die Anfrage aus. Wirft nicht; Fehler kommen als HttpErrorKind
    // zurück.
    virtual TransportResult send(const std::string& url,
                                 const HttpRequest& request,
                                 const TlsPolicy& policy) = 0;
};

}  // namespace baludesk::core
