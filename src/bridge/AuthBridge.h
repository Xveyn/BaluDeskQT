#pragma once

#include <QObject>
#include <QString>

#include <memory>
#include <string>

#include "core/auth/AuthenticatedClient.h"
#include "core/auth/SessionRepository.h"
#include "core/util/SecretStore.h"

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

    // Sitzung entfernen. Meldet loggedOut oder logoutIncomplete.
    void logout();

signals:
    void noSession();
    void sessionEstablished(const QString& serverUrl, const QString& username);
    void serverUnreachable(const QString& message);

    // Der angeheftete Schlüssel passt nicht mehr, oder ein Zertifikat ohne
    // Pin lässt sich nicht mehr prüfen. Beides endet auf derselben Seite,
    // braucht dort aber verschiedene Sätze - daher der Grund als zweites
    // Argument.
    void certificateChanged(const QString& serverUrl, const QString& reason);

    void reauthRequired(const QString& message);
    void loggedOut();
    void logoutIncomplete(const QString& message);

    // Die Signale des Paarungsablaufs kommen in Task 6 dazu, gemeinsam mit
    // den Slots, die sie melden.

private:
    std::shared_ptr<core::Transport> transport_;

    // REIHENFOLGE IST TRAGEND: FileSessionRepository hält eine SecretStore&.
    // Mitglieder werden in umgekehrter Deklarationsreihenfolge zerstört, also
    // stirbt repository_ vor secrets_. Wer die beiden Zeilen tauscht, baut
    // eine Referenz, die ihren Bezug überlebt - und zwar still.
    std::unique_ptr<core::SecretStore> secrets_;
    std::unique_ptr<core::SessionRepository> repository_;

    // Werden erst in Task 6 gelesen; der Konstruktor nimmt sie schon hier
    // entgegen, weil er sonst in Task 6 seine Signatur ändern müsste.
    QString deviceId_;
    QString deviceName_;

    std::unique_ptr<core::HttpClient> http_;
    std::unique_ptr<core::AuthenticatedClient> authenticated_;

    // trust_, pairing_, pollTimer_ und pendingServerUrl_ kommen in Task 6
    // dazu.
};

}  // namespace baludesk::bridge
