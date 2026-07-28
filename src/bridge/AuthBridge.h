#pragma once

#include <QObject>
#include <QString>

#include <memory>
#include <string>

#include "core/api/TrustNegotiation.h"
#include "core/auth/AuthenticatedClient.h"
#include "core/auth/PairingClient.h"
#include "core/auth/SessionRepository.h"
#include "core/util/SecretStore.h"

class QTimer;

namespace baludesk::core {
class HttpClient;
class Transport;
}  // namespace baludesk::core

namespace baludesk::bridge {

// Die einzige Stelle im Programm, an der beide Welten sich berühren
// (Spec Abschnitt 5).
//
// Sie besitzt alles, was blockiert: HttpClient blockiert bis zu 30 Sekunden
// und lässt sich nicht abbrechen (Spec 2.10). Die Oberfläche redet
// ausschliesslich über Signale und Slots mit ihr und fasst nie einen
// core-Typ an, der ins Netz greift.
//
// DIESE KLASSE ERZEUGT KEINEN THREAD UND KENNT KEINEN. Wo sie lebt,
// entscheidet ihr Eigentümer - in der Anwendung schiebt MainWindow sie per
// moveToThread in einen eigenen QThread, in den Tests bleibt sie, wo sie ist.
// Dadurch liegt die Threading-Entscheidung an einer einzigen sichtbaren
// Stelle, und die Klasse ist ohne Ereignisschleife prüfbar.
//
// Ebenfalls bewusst: keine Widgets. Diese Bibliothek linkt Qt6::Core, nicht
// Qt6::Widgets - ein QMessageBox im Arbeiterthread wäre sonst ein Fehler, der
// sich erst zur Laufzeit zeigt.
class AuthBridge : public QObject {
    Q_OBJECT

public:
    // transport wird geteilt (HttpClient hält ihn als shared_ptr); secrets
    // geht in den Besitz der Bridge über.
    AuthBridge(std::shared_ptr<core::Transport> transport,
               std::unique_ptr<core::SecretStore> secrets,
               std::string sessionPath,
               QString deviceId,
               QString deviceName,
               QObject* parent = nullptr);

    ~AuthBridge() override;

public slots:
    // Beim Start: abgelegte Sitzung laden und beim Server nachfragen, ob sie
    // noch trägt. Meldet genau eines von noSession, sessionEstablished,
    // reauthRequired, certificateChanged oder serverUnreachable.
    void restoreSession();

    // Adresse prüfen, Vertrauen klären, Paarung beginnen.
    void connectToServer(const QString& rawUrl);

    // Nach certificateConfirmationRequired: den gezeigten Schlüssel anheften
    // und weitermachen.
    void confirmCertificate();

    // Nach certificateConfirmationRequired: nichts anheften, Vorgang beenden.
    void rejectCertificate();

    // Während der Codeanzeige: Vorgang beenden. Der Gerätecode verfällt beim
    // Server von selbst, es ist nichts zurückzunehmen.
    void cancelPairing();

    // Ein Abfrageschritt. Wird vom Zeitgeber aufgerufen.
    //
    // Öffentlich, damit die Tests den Ablauf ohne Ereignisschleife fahren
    // können - sonst wäre dieser Schritt nur über echte verstreichende
    // Sekunden erreichbar, und die Abfragefolge bliebe ungeprüft.
    void pollNow();

    // Sitzung entfernen. Meldet loggedOut oder logoutIncomplete.
    void logout();

signals:
    void noSession();
    void sessionEstablished(const QString& serverUrl, const QString& username);

    // Trägt Adresse und Benutzernamen mit, weil die Oberfläche daraufhin die
    // verbundene Ansicht mit Offline-Hinweis zeigt statt der Eingabeseite:
    // ein schlafender Server ist laut Spec Abschnitt 10 Betriebszustand, und
    // wer den Nutzer dafür zur Serveradresse zurückschickt, sagt ihm, er sei
    // nicht gepaart. Beide Werte sind belegt, weil dieses Signal nur bei
    // vorhandener Sitzung entsteht.
    void serverUnreachable(const QString& serverUrl,
                           const QString& username,
                           const QString& message);

    // Der angeheftete Schlüssel passt nicht mehr, oder ein Zertifikat ohne
    // Pin lässt sich nicht mehr prüfen. Beides endet auf derselben Seite,
    // braucht dort aber verschiedene Sätze - daher der Grund als zweites
    // Argument.
    //
    // Den Fall unterscheiden kann der Grund allein nicht: er ist der Klartext
    // der Gegenstelle und sagt nichts darüber, ob je ein Schlüssel angeheftet
    // wurde. Genau das trägt hadPinnedKey. Ohne dieses dritte Argument
    // behauptet die Seite auch dann, der Server weise ein anderes Zertifikat
    // vor „als das beim Verbinden bestätigte", wenn nie etwas bestätigt und
    // nichts angeheftet wurde - bei einem öffentlich vertrauenswürdigen
    // Zertifikat, das schlicht abgelaufen ist, ist das eine falsche Diagnose.
    void certificateChanged(const QString& serverUrl,
                            const QString& reason,
                            bool hadPinnedKey);

    void reauthRequired(const QString& message);
    void loggedOut();
    void logoutIncomplete(const QString& message);

    void serverUrlRejected(const QString& message);
    void certificateConfirmationRequired(const QString& fingerprint, const QString& reason);
    void pairingCodeReady(const QString& userCode,
                          const QString& verificationUrl,
                          int expiresInSeconds);
    void pairingNotice(const QString& message);
    void pairingFailed(const QString& message);
    void pairingCancelled();

private:
    void beginPairing();
    void finishPairing();
    void resetPairing();

    std::shared_ptr<core::Transport> transport_;

    // REIHENFOLGE IST TRAGEND: FileSessionRepository hält eine SecretStore&.
    // Mitglieder werden in umgekehrter Deklarationsreihenfolge zerstört, also
    // stirbt repository_ vor secrets_. Wer die beiden Zeilen tauscht, baut
    // eine Referenz, die ihren Bezug überlebt - und zwar still.
    std::unique_ptr<core::SecretStore> secrets_;
    std::unique_ptr<core::SessionRepository> repository_;

    QString deviceId_;
    QString deviceName_;

    std::unique_ptr<core::HttpClient> http_;
    std::unique_ptr<core::AuthenticatedClient> authenticated_;

    std::unique_ptr<core::TrustNegotiation> trust_;
    std::unique_ptr<core::PairingClient> pairing_;

    QTimer* pollTimer_ = nullptr;
    QString pendingServerUrl_;
};

}  // namespace baludesk::bridge
