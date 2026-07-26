#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "core/api/HttpTypes.h"
#include "core/api/TlsPolicy.h"
#include "core/api/Transport.h"

namespace baludesk::core::test {

// Merkt sich, womit er aufgerufen wurde, und liefert der Reihe nach
// vorgegebene Ergebnisse aus einer Warteschlange.
//
// Eine einzelne Antwort reicht für HttpClient-Tests, aber Stufe 1b braucht
// mehr: ihre Abfrageschleife beim Pairing ist ihrem Wesen nach eine Folge -
// "authorization_pending, authorization_pending, approved" - und lässt sich
// gegen ein Fake mit nur einem festen Ergebnis gar nicht prüfen. Ist die
// Warteschlange erschöpft, wiederholt sich das letzte eingereihte Ergebnis;
// ein Test, der nur ein einziges Ergebnis einreiht, verhält sich deshalb
// unverändert wie vor der Warteschlange.
class FakeTransport : public Transport {
public:
    struct Call {
        std::string url;
        HttpRequest request;
        TlsPolicy policy;
    };

    std::vector<Call> calls;

    // Reiht ein weiteres Ergebnis ans Ende der Warteschlange ein.
    void queueResult(TransportResult result) { results_.push_back(std::move(result)); }

    TransportResult send(const std::string& url,
                         const HttpRequest& request,
                         const TlsPolicy& policy) override {
        calls.push_back(Call{url, request, policy});

        if (results_.empty()) {
            // Kein Ergebnis eingereiht: ein leeres TransportResult
            // (HttpErrorKind::None, Status 0) statt eines Absturzes.
            return TransportResult{};
        }

        const std::size_t index = calls.size() - 1;
        const std::size_t clampedIndex = index < results_.size() ? index : results_.size() - 1;
        return results_[clampedIndex];
    }

private:
    std::vector<TransportResult> results_;
};

}  // namespace baludesk::core::test
