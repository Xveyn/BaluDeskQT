#pragma once

#include <optional>
#include <string>

namespace baludesk::core {

class HttpClient;

// Wo die Vertrauensentscheidung über ein Serverzertifikat steht.
enum class TrustState {
    // Noch nichts geprüft.
    Idle,
    // Regulär geprüft oder der angeheftete Schlüssel passt. Verbindung steht.
    Trusted,
    // Unbekanntes Zertifikat. prompt() ist belegt, ein Mensch muss den
    // Fingerabdruck vergleichen.
    ConfirmationRequired,
    // Der angeheftete Schlüssel passt nicht mehr. Spec Abschnitt 2.9: harter
    // Abbruch, keine Nachfrage.
    PinMismatch,
    // Netz weg, Zeitlimit. Sagt nichts über das Zertifikat aus.
    Unreachable,
    // Zertifikat unlesbar oder gar nicht auszulesen, unerwartete Antwort.
    Failed
};

// Was dem Nutzer zu zeigen ist, damit er entscheiden kann.
struct TrustPrompt {
    // Zum Anzeigen und Vergleichen, in der Darstellung, die auch Browser und
    // die openssl-Kommandozeile ausgeben.
    std::string fingerprintSha256;

    // Zum Anheften, im Format, das libcurl erwartet.
    std::string publicKeyPin;

    // Warum die reguläre Prüfung fehlschlug, im Klartext der Gegenstelle.
    std::string reason;
};

// Der Ablauf „prüfen - bei Bedarf bestätigen lassen - anheften - erneut
// prüfen" als Zustandsobjekt ohne jede Ein- und Ausgabe.
//
// Die Klasse fragt niemanden und schreibt nichts: sie sagt, was zu tun ist,
// und wartet auf die Antwort. Genau deshalb kann derselbe Ablauf einmal von
// einem Dialog und einmal vom Prüfprogramm gefahren werden, ohne dass einer
// von beiden ihn nachbaut - und deshalb ist er ohne Netz prüfbar.
//
// Die Klasse setzt die TLS-Regel des übergebenen HttpClient. Sie besitzt ihn
// nicht; er muss sie überleben.
class TrustNegotiation {
public:
    explicit TrustNegotiation(HttpClient& http);

    // Nimmt mit der aktuell gesetzten Regel Kontakt auf.
    TrustState probe();

    // Heftet den angebotenen Schlüssel an und prüft erneut. Nur nach
    // ConfirmationRequired wirksam; sonst bleibt alles, wie es war, und es
    // wird nichts gesendet.
    //
    // Der Pin gilt erst dann als bestätigt, wenn die erneute Prüfung
    // Trusted ergibt - ein Pin, der nie zu einer stehenden Verbindung
    // geführt hat, gehört nicht in die Sitzung. Scheitert die erneute
    // Prüfung, wird die TLS-Regel des HttpClient auf volle Prüfung
    // zurückgesetzt statt auf dem eben angehefteten, nie bestätigten
    // Schlüssel stehen zu bleiben.
    TrustState confirm();

    [[nodiscard]] const std::optional<TrustPrompt>& prompt() const { return prompt_; }

    // Leer, solange nichts bestätigt wurde - und ebenso bei regulärer
    // Prüfung, wo es nichts anzuheften gibt. Genau dieser Wert gehört in
    // Session::publicKeyPin.
    [[nodiscard]] const std::string& confirmedPin() const { return confirmedPin_; }

    [[nodiscard]] TrustState state() const { return state_; }

    // Erklärung im Klartext, zur Anzeige geeignet. Leer, wenn alles stimmt.
    [[nodiscard]] const std::string& message() const { return message_; }

private:
    // Ein Kontaktversuch samt Auswertung. Setzt state_, prompt_ und message_.
    TrustState evaluate();

    TrustState settle(TrustState state, std::string message);

    HttpClient& http_;

    TrustState state_ = TrustState::Idle;
    std::optional<TrustPrompt> prompt_;
    std::string confirmedPin_;
    std::string message_;
};

}  // namespace baludesk::core
