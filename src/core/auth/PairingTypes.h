#pragma once

#include <string>

namespace baludesk::core {

// Wo der Paarungsvorgang gerade steht.
enum class PairingState {
    // Noch nicht begonnen.
    Idle,
    // Code liegt vor, es wird auf die Freigabe im Browser gewartet.
    WaitingForApproval,
    // Freigegeben, Token liegen vor.
    Approved,
    // Im Browser abgelehnt.
    Denied,
    // Der Code ist verfallen oder der Server kennt ihn nicht mehr.
    Expired,
    // Endgültig gescheitert - siehe Meldung.
    Failed
};

// Was dem Nutzer gezeigt werden muss, damit er die Paarung freigeben kann.
struct PairingPrompt {
    // Sechsstellig, wird im Browser eingegeben.
    std::string userCode;
    // Adresse, an der die Freigabe stattfindet.
    std::string verificationUrl;
    // Restlaufzeit des Codes ab Ausstellung.
    int expiresInSeconds = 0;
};

// Das Ergebnis einer erfolgreichen Paarung.
//
// ACHTUNG: Der Server liefert diese Werte genau einmal aus und löscht dabei
// seinen Datensatz. Ein erneuter Abruf mit demselben Gerätecode liefert 404.
// Wer sie verliert, muss neu paaren - sie gehören deshalb abgelegt, bevor
// irgendetwas anderes geschieht.
struct PairingTokens {
    std::string accessToken;
    std::string refreshToken;
    std::string username;
};

// Was nach einem Schritt zu tun ist.
struct PairingProgress {
    PairingState state = PairingState::Idle;

    // Sekunden bis zum nächsten poll(). 0, wenn nicht mehr zu fragen ist.
    int waitSeconds = 0;

    // Erklärung im Klartext, zur Anzeige geeignet. Leer, solange alles
    // seinen normalen Gang geht.
    std::string message;
};

}  // namespace baludesk::core
