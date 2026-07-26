#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "core/auth/PairingTypes.h"

namespace baludesk::core {

class HttpClient;

// Der Gerätecode-Ablauf als Zustandsautomat.
//
// Die Klasse schläft nie und kennt die Uhr nicht: die aktuelle Zeit wird ihr
// hereingereicht, und sie gibt zurück, nach wie vielen Sekunden sie wieder
// gefragt werden möchte. Dadurch lässt sich das gesamte Zeitverhalten prüfen,
// ohne dass ein Test auch nur eine Sekunde wartet, und die Oberfläche kann
// poll() an einen Zeitgeber hängen statt einen Thread zu blockieren.
//
// Das vom Server gelieferte Intervall ist einzuhalten und nicht zu
// unterbieten: bei 12 zugelassenen Abrufen je Minute und einem vorgegebenen
// Intervall von 5 Sekunden gibt es keine Reserve.
class PairingClient {
public:
    explicit PairingClient(HttpClient& http);

    // Fordert einen Gerätecode an. Danach steht prompt() bereit.
    // nowSeconds ist eine beliebige, monoton wachsende Sekundenzählung.
    PairingProgress begin(const std::string& deviceId,
                          const std::string& deviceName,
                          std::int64_t nowSeconds);

    // Fragt einmal nach dem Stand. Erst aufrufen, wenn seit dem letzten
    // Ergebnis so viele Sekunden vergangen sind, wie es angab.
    //
    // Nach einem Endzustand - Approved, Denied, Expired, Failed - werden
    // keine Anfragen mehr gestellt; der erreichte Zustand wird unverändert
    // zurückgegeben. Das ist wichtig, weil ein zweiter Abruf nach der
    // Freigabe nur ein 404 brächte.
    PairingProgress poll(std::int64_t nowSeconds);

    [[nodiscard]] const std::optional<PairingPrompt>& prompt() const { return prompt_; }
    [[nodiscard]] const std::optional<PairingTokens>& tokens() const { return tokens_; }
    [[nodiscard]] PairingState state() const { return state_; }

private:
    PairingProgress finish(PairingState state, std::string message);
    PairingProgress waiting(int waitSeconds, std::string message = {});

    HttpClient& http_;

    PairingState state_ = PairingState::Idle;
    std::optional<PairingPrompt> prompt_;
    std::optional<PairingTokens> tokens_;

    std::string deviceCode_;
    int intervalSeconds_ = 5;
    std::int64_t deadlineSeconds_ = 0;
};

}  // namespace baludesk::core
