#include "bridge/AuthBridge.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QFile>
#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>

#include "core/api/HttpTypes.h"
#include "core/auth/FileSessionRepository.h"
#include "core/support/FakeSecretStore.h"
#include "core/support/FakeTransport.h"

using baludesk::bridge::AuthBridge;
using baludesk::core::HttpErrorKind;
using baludesk::core::TransportResult;
using baludesk::core::test::FakeSecretStore;
using baludesk::core::test::FakeTransport;

namespace {

// QObject und QTimer verlangen keine laufende Ereignisschleife, aber ohne eine
// QCoreApplication-Instanz warnt Qt beim Anlegen eines Zeitgebers. Eine
// einzige, statisch gehaltene Instanz für alle Tests - eine zweite anzulegen
// wäre ein fataler Fehler in Qt.
QCoreApplication& application() {
    static int argc = 1;
    static char argName[] = "baludesk_app_tests";
    static char* argv[] = {argName, nullptr};
    static QCoreApplication instance(argc, argv);
    return instance;
}

TransportResult jsonResult(long statusCode, std::string body) {
    TransportResult result;
    result.error = HttpErrorKind::None;
    result.response.statusCode = statusCode;
    result.response.body = std::move(body);
    return result;
}

TransportResult failure(HttpErrorKind kind, std::string detail) {
    TransportResult result;
    result.error = kind;
    result.detail = std::move(detail);
    return result;
}

// Legt eine vollständige Sitzung in Datei und Geheimnisspeicher ab, wie sie
// nach einer echten Paarung dastünde.
void storeSession(const std::string& jsonPath, FakeSecretStore& secrets) {
    baludesk::core::Session session;
    session.serverUrl = "https://nas.example.com";
    session.username = "sven";
    session.publicKeyPin.clear();
    session.accessToken = "zugangstoken";
    session.refreshToken = "erneuerungstoken";

    baludesk::core::FileSessionRepository repository(jsonPath, secrets);
    ASSERT_TRUE(repository.save(session));
}

// Hält alles beisammen, was eine Bridge zum Leben braucht, und gibt den
// Geheimnisspeicher zusätzlich als rohe Referenz heraus - die Bridge besitzt
// ihn, der Test will ihn trotzdem befragen.
struct Fixture {
    QTemporaryDir directory;
    std::shared_ptr<FakeTransport> transport = std::make_shared<FakeTransport>();
    FakeSecretStore* secrets = nullptr;
    std::unique_ptr<AuthBridge> bridge;

    Fixture() {
        application();
        EXPECT_TRUE(directory.isValid());

        auto owned = std::make_unique<FakeSecretStore>();
        secrets = owned.get();

        bridge = std::make_unique<AuthBridge>(transport,
                                              std::move(owned),
                                              sessionPath(),
                                              QStringLiteral("geraet-1"),
                                              QStringLiteral("Svens Rechner"));
    }

    [[nodiscard]] std::string sessionPath() const {
        return directory.filePath(QStringLiteral("session.json")).toStdString();
    }
};

const char* kDeviceCodeResponse = R"({
  "device_code": "geheimer-geraetecode",
  "user_code": "483920",
  "verification_url": "https://nas.example.com/devices?pair=1",
  "expires_in": 600,
  "interval": 5
})";

const char* kApprovedResponse = R"({
  "status": "approved",
  "access_token": "zugangstoken",
  "refresh_token": "erneuerungstoken",
  "token_type": "bearer",
  "user": {"id": 1, "username": "sven", "email": "sven@example.com", "role": "admin"}
})";

// Ein echtes Zertifikat, damit identifyCertificate() etwas zu tun bekommt.
std::string testCertificate() {
    const std::filesystem::path path =
        std::filesystem::path(BALUDESK_TEST_DATA_DIR) / "test-cert-a.pem";
    std::ifstream stream(path, std::ios::binary);
    EXPECT_TRUE(stream.good()) << "Testdatei nicht lesbar: " << path.string();
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

TransportResult untrustedResult(std::string pem) {
    TransportResult result;
    result.error = HttpErrorKind::CertificateUntrusted;
    result.detail = "self signed certificate";
    result.peerCertificatePem = std::move(pem);
    return result;
}

}  // namespace

TEST(AuthBridgeTest, ReportsNoSessionWhenNothingIsStored) {
    Fixture f;
    QSignalSpy none(f.bridge.get(), &AuthBridge::noSession);

    f.bridge->restoreSession();

    EXPECT_EQ(none.count(), 1);
    // Ohne Sitzung gibt es nichts zu fragen - der Server wird nicht behelligt.
    EXPECT_TRUE(f.transport->calls.empty());
}

TEST(AuthBridgeTest, EstablishesTheStoredSessionWhenTheServerAcceptsIt) {
    Fixture f;
    storeSession(f.sessionPath(), *f.secrets);
    f.transport->queueResult(jsonResult(200, R"({"username":"sven"})"));

    QSignalSpy established(f.bridge.get(), &AuthBridge::sessionEstablished);
    f.bridge->restoreSession();

    ASSERT_EQ(established.count(), 1);
    EXPECT_EQ(established.at(0).at(0).toString(), QStringLiteral("https://nas.example.com"));
    EXPECT_EQ(established.at(0).at(1).toString(), QStringLiteral("sven"));

    ASSERT_EQ(f.transport->calls.size(), std::size_t{1});
    EXPECT_EQ(f.transport->calls[0].url, "https://nas.example.com/api/auth/me");
}

TEST(AuthBridgeTest, AsksToReauthenticateWhenTheRefreshTokenIsDead) {
    Fixture f;
    storeSession(f.sessionPath(), *f.secrets);
    f.transport->queueResult(jsonResult(401, "{}"));
    f.transport->queueResult(jsonResult(401, R"({"detail":"Invalid refresh token"})"));

    QSignalSpy reauth(f.bridge.get(), &AuthBridge::reauthRequired);
    QSignalSpy established(f.bridge.get(), &AuthBridge::sessionEstablished);
    f.bridge->restoreSession();

    EXPECT_EQ(established.count(), 0);
    ASSERT_EQ(reauth.count(), 1);
    EXPECT_FALSE(reauth.at(0).at(0).toString().isEmpty());
}

TEST(AuthBridgeTest, KeepsTheSessionWhenTheServerIsAsleep) {
    Fixture f;
    storeSession(f.sessionPath(), *f.secrets);
    f.transport->queueResult(failure(HttpErrorKind::Network, "connection refused"));

    QSignalSpy unreachable(f.bridge.get(), &AuthBridge::serverUnreachable);
    QSignalSpy reauth(f.bridge.get(), &AuthBridge::reauthRequired);
    f.bridge->restoreSession();

    // Ein schlafender Server ist laut Spec Abschnitt 10 Betriebszustand. Die
    // Sitzung deshalb zu verwerfen wäre der teuerste denkbare Fehlschluss.
    EXPECT_EQ(unreachable.count(), 1);
    EXPECT_EQ(reauth.count(), 0);
    EXPECT_TRUE(f.secrets->contains("session/access-token"));

    // Adresse und Benutzername gehen mit, damit die Oberfläche die verbundene
    // Ansicht mit Offline-Hinweis zeigen kann statt der Eingabeseite. Ohne
    // diese Nutzlast wüsste sie nicht, mit wem sie gerade nicht redet - und
    // müsste den Nutzer fragen, wo sein Server steht, obwohl er längst
    // gepaart ist.
    ASSERT_EQ(unreachable.at(0).size(), 3);
    EXPECT_EQ(unreachable.at(0).at(0).toString(), QStringLiteral("https://nas.example.com"));
    EXPECT_EQ(unreachable.at(0).at(1).toString(), QStringLiteral("sven"));
    EXPECT_FALSE(unreachable.at(0).at(2).toString().isEmpty());
}

TEST(AuthBridgeTest, ReportsAChangedCertificate) {
    Fixture f;
    storeSession(f.sessionPath(), *f.secrets);
    f.transport->queueResult(failure(HttpErrorKind::CertificatePinMismatch, "pinned key mismatch"));

    QSignalSpy changed(f.bridge.get(), &AuthBridge::certificateChanged);
    f.bridge->restoreSession();

    ASSERT_EQ(changed.count(), 1);
    EXPECT_EQ(changed.at(0).at(0).toString(), QStringLiteral("https://nas.example.com"));
    EXPECT_FALSE(changed.at(0).at(1).toString().isEmpty());
    // Kein zweiter Aufruf: keine Erneuerung über eine Verbindung, der gerade
    // nicht zu trauen ist.
    EXPECT_EQ(f.transport->calls.size(), std::size_t{1});
}

TEST(AuthBridgeTest, LogoutClearsTheSessionAndSaysSo) {
    Fixture f;
    storeSession(f.sessionPath(), *f.secrets);

    QSignalSpy loggedOut(f.bridge.get(), &AuthBridge::loggedOut);
    f.bridge->logout();

    EXPECT_EQ(loggedOut.count(), 1);
    EXPECT_TRUE(f.secrets->entries.empty());
    EXPECT_FALSE(QFile::exists(f.directory.filePath(QStringLiteral("session.json"))));
}

TEST(AuthBridgeTest, LogoutSaysSoWhenSomethingRemained) {
    Fixture f;
    storeSession(f.sessionPath(), *f.secrets);
    f.secrets->failOnEraseNumber = 1;

    QSignalSpy incomplete(f.bridge.get(), &AuthBridge::logoutIncomplete);
    QSignalSpy loggedOut(f.bridge.get(), &AuthBridge::loggedOut);
    f.bridge->logout();

    // „Erledigt" zu melden, während ein Token im Speicher steht, wäre eine
    // Unwahrheit an genau der Stelle, an der jemand seine Spuren löschen will.
    EXPECT_EQ(loggedOut.count(), 0);
    ASSERT_EQ(incomplete.count(), 1);
    EXPECT_FALSE(incomplete.at(0).at(0).toString().isEmpty());
}

TEST(AuthBridgeTest, RejectsABadServerUrlWithoutTouchingTheNetwork) {
    Fixture f;
    QSignalSpy rejected(f.bridge.get(), &AuthBridge::serverUrlRejected);

    f.bridge->connectToServer(QStringLiteral("http://nas.example.com"));

    // Über diese Verbindung ginge ein Zugangstoken. Die Prüfung liegt in
    // core::checkServerUrl und greift vor jedem Netzkontakt.
    ASSERT_EQ(rejected.count(), 1);
    EXPECT_FALSE(rejected.at(0).at(0).toString().isEmpty());
    EXPECT_TRUE(f.transport->calls.empty());
}

TEST(AuthBridgeTest, EmitsTheCodeWhenTheCertificateIsFine) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, R"({"status":"ok"})"));
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));

    QSignalSpy ready(f.bridge.get(), &AuthBridge::pairingCodeReady);
    f.bridge->connectToServer(QStringLiteral("nas.example.com"));

    ASSERT_EQ(ready.count(), 1);
    EXPECT_EQ(ready.at(0).at(0).toString(), QStringLiteral("483920"));
    EXPECT_EQ(ready.at(0).at(1).toString(),
              QStringLiteral("https://nas.example.com/devices?pair=1"));
    EXPECT_EQ(ready.at(0).at(2).toInt(), 600);

    ASSERT_EQ(f.transport->calls.size(), std::size_t{2});
    EXPECT_EQ(f.transport->calls[0].url, "https://nas.example.com/api/health");
    EXPECT_EQ(f.transport->calls[1].url,
              "https://nas.example.com/api/desktop-pairing/device-code");
    // Die Gerätekennung geht so mit, wie sie hereingereicht wurde.
    EXPECT_NE(f.transport->calls[1].request.jsonBody.find("geraet-1"), std::string::npos);
    EXPECT_NE(f.transport->calls[1].request.jsonBody.find("Svens Rechner"), std::string::npos);
}

TEST(AuthBridgeTest, AsksForCertificateConfirmationOnAnUnknownCertificate) {
    Fixture f;
    f.transport->queueResult(untrustedResult(testCertificate()));

    QSignalSpy confirmation(f.bridge.get(), &AuthBridge::certificateConfirmationRequired);
    QSignalSpy ready(f.bridge.get(), &AuthBridge::pairingCodeReady);
    f.bridge->connectToServer(QStringLiteral("nas.local"));

    ASSERT_EQ(confirmation.count(), 1);
    EXPECT_FALSE(confirmation.at(0).at(0).toString().isEmpty());
    EXPECT_EQ(confirmation.at(0).at(1).toString(), QStringLiteral("self signed certificate"));
    // Vor der Entscheidung wird nicht gepaart.
    EXPECT_EQ(ready.count(), 0);
    EXPECT_EQ(f.transport->calls.size(), std::size_t{1});
}

TEST(AuthBridgeTest, ConfirmingTheCertificatePinsItAndContinues) {
    Fixture f;
    f.transport->queueResult(untrustedResult(testCertificate()));
    f.bridge->connectToServer(QStringLiteral("nas.local"));

    f.transport->queueResult(jsonResult(200, R"({"status":"ok"})"));
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));

    QSignalSpy ready(f.bridge.get(), &AuthBridge::pairingCodeReady);
    f.bridge->confirmCertificate();

    EXPECT_EQ(ready.count(), 1);
    ASSERT_EQ(f.transport->calls.size(), std::size_t{3});
    // Ab der erneuten Prüfung trägt jeder Aufruf die angeheftete Regel.
    EXPECT_EQ(f.transport->calls[1].policy.mode,
              baludesk::core::TlsPolicy::Mode::Pinned);
    EXPECT_EQ(f.transport->calls[2].policy.mode,
              baludesk::core::TlsPolicy::Mode::Pinned);
}

TEST(AuthBridgeTest, RejectingTheCertificateEndsTheAttempt) {
    Fixture f;
    f.transport->queueResult(untrustedResult(testCertificate()));
    f.bridge->connectToServer(QStringLiteral("nas.local"));
    const std::size_t callsBefore = f.transport->calls.size();

    QSignalSpy cancelled(f.bridge.get(), &AuthBridge::pairingCancelled);
    f.bridge->rejectCertificate();

    EXPECT_EQ(cancelled.count(), 1);
    // Nichts angeheftet, nichts gesendet.
    EXPECT_EQ(f.transport->calls.size(), callsBefore);
}

TEST(AuthBridgeTest, StoresTheSessionOnApproval) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, R"({"status":"ok"})"));
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.bridge->connectToServer(QStringLiteral("nas.example.com"));

    f.transport->queueResult(jsonResult(200, kApprovedResponse));

    QSignalSpy established(f.bridge.get(), &AuthBridge::sessionEstablished);
    f.bridge->pollNow();

    ASSERT_EQ(established.count(), 1);
    EXPECT_EQ(established.at(0).at(0).toString(), QStringLiteral("https://nas.example.com"));
    EXPECT_EQ(established.at(0).at(1).toString(), QStringLiteral("sven"));

    // Die Token liegen nachweislich in der Ablage - der Server liefert sie
    // kein zweites Mal aus.
    EXPECT_EQ(f.secrets->entries["session/access-token"], "zugangstoken");
    EXPECT_EQ(f.secrets->entries["session/refresh-token"], "erneuerungstoken");
    EXPECT_TRUE(QFile::exists(f.directory.filePath(QStringLiteral("session.json"))));
}

TEST(AuthBridgeTest, StoresTheConfirmedPinWithTheSession) {
    Fixture f;
    f.transport->queueResult(untrustedResult(testCertificate()));
    f.bridge->connectToServer(QStringLiteral("nas.local"));
    f.transport->queueResult(jsonResult(200, R"({"status":"ok"})"));
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.bridge->confirmCertificate();

    f.transport->queueResult(jsonResult(200, kApprovedResponse));
    f.bridge->pollNow();

    // Ohne den Pin in der Sitzung verbindet sich der Client beim nächsten
    // Start nicht mehr - die Bestätigung wäre für einen Programmlauf gewesen.
    baludesk::core::FileSessionRepository repository(f.sessionPath(), *f.secrets);
    const auto stored = repository.load();
    ASSERT_TRUE(stored.has_value());
    EXPECT_FALSE(stored->publicKeyPin.empty());
}

TEST(AuthBridgeTest, ReportsAFailureWhenTheSessionCannotBeStored) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, R"({"status":"ok"})"));
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.bridge->connectToServer(QStringLiteral("nas.example.com"));

    f.transport->queueResult(jsonResult(200, kApprovedResponse));
    f.secrets->failOnSaveNumber = 1;

    QSignalSpy failed(f.bridge.get(), &AuthBridge::pairingFailed);
    QSignalSpy established(f.bridge.get(), &AuthBridge::sessionEstablished);
    f.bridge->pollNow();

    // Der Server hat seinen Datensatz beim Ausliefern gelöscht. Was hier
    // verlorengeht, ist endgültig verloren - das gehört gesagt, nicht
    // verschwiegen.
    EXPECT_EQ(established.count(), 0);
    ASSERT_EQ(failed.count(), 1);
    EXPECT_FALSE(failed.at(0).at(0).toString().isEmpty());
}

TEST(AuthBridgeTest, CancelDuringPollingStopsTheAttempt) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, R"({"status":"ok"})"));
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.bridge->connectToServer(QStringLiteral("nas.example.com"));
    const std::size_t callsBefore = f.transport->calls.size();

    QSignalSpy cancelled(f.bridge.get(), &AuthBridge::pairingCancelled);
    f.bridge->cancelPairing();

    EXPECT_EQ(cancelled.count(), 1);

    // Nach dem Abbruch fragt auch ein Abfrageschritt nicht mehr nach.
    f.bridge->pollNow();
    EXPECT_EQ(f.transport->calls.size(), callsBefore);
}

TEST(AuthBridgeTest, CancelWithoutARunningPairingSaysNothing) {
    Fixture f;
    QSignalSpy cancelled(f.bridge.get(), &AuthBridge::pairingCancelled);

    f.bridge->cancelPairing();

    // Die Oberfläche stellt ihre Aufrufe in die Warteschlange dieses Threads.
    // Ein Klick auf „Abbrechen", der erst abgearbeitet wird, nachdem die
    // Paarung durch ist, meldete sonst einen Abbruch - und die Oberfläche
    // wechselt darauf bedingungslos zur Eingabeseite zurück.
    EXPECT_EQ(cancelled.count(), 0);
}

TEST(AuthBridgeTest, ALateCancelLeavesTheEstablishedSessionAlone) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, R"({"status":"ok"})"));
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.bridge->connectToServer(QStringLiteral("nas.example.com"));

    f.transport->queueResult(jsonResult(200, kApprovedResponse));
    QSignalSpy established(f.bridge.get(), &AuthBridge::sessionEstablished);
    f.bridge->pollNow();
    ASSERT_EQ(established.count(), 1);

    QSignalSpy cancelled(f.bridge.get(), &AuthBridge::pairingCancelled);
    f.bridge->cancelPairing();

    // Die Token liegen auf der Platte. Ein Abbruch, der jetzt noch gemeldet
    // würde, liesse die Anwendung das Gegenteil behaupten.
    EXPECT_EQ(cancelled.count(), 0);
    EXPECT_TRUE(QFile::exists(f.directory.filePath(QStringLiteral("session.json"))));
}

TEST(AuthBridgeTest, RejectingWithoutAPendingCertificateSaysNothing) {
    Fixture f;
    QSignalSpy cancelled(f.bridge.get(), &AuthBridge::pairingCancelled);

    f.bridge->rejectCertificate();

    EXPECT_EQ(cancelled.count(), 0);
}

TEST(AuthBridgeTest, AMismatchingPinEndsTheAttemptAndNotOnlyReportsIt) {
    Fixture f;
    f.transport->queueResult(failure(HttpErrorKind::CertificatePinMismatch, "pinned key mismatch"));

    QSignalSpy changed(f.bridge.get(), &AuthBridge::certificateChanged);
    f.bridge->connectToServer(QStringLiteral("nas.example.com"));
    ASSERT_EQ(changed.count(), 1);

    // Bei abweichendem Pin bricht die Verbindung hart ab (Spec 2.9). Bliebe
    // der Vorgang stehen, fände ihn ein nachlaufender Klick vor - und der
    // Nutzer landete über pairingCancelled wieder in einem Ablauf, den es
    // nicht mehr gibt.
    QSignalSpy cancelled(f.bridge.get(), &AuthBridge::pairingCancelled);
    f.bridge->rejectCertificate();
    EXPECT_EQ(cancelled.count(), 0);
}

TEST(AuthBridgeTest, LogoutWorksAfterAMismatchingPin) {
    Fixture f;
    storeSession(f.sessionPath(), *f.secrets);
    f.transport->queueResult(failure(HttpErrorKind::CertificatePinMismatch, "pinned key mismatch"));
    f.bridge->connectToServer(QStringLiteral("nas.example.com"));

    QSignalSpy loggedOut(f.bridge.get(), &AuthBridge::loggedOut);
    f.bridge->logout();

    // Der Weg, auf dem die Zertifikatsseite ins Abmelden führt. Er ist der
    // einzige, auf dem trust_ und http_ beim Abmelden beide noch leben -
    // deshalb steht er hier festgeschrieben.
    EXPECT_EQ(loggedOut.count(), 1);
    EXPECT_TRUE(f.secrets->entries.empty());
}

TEST(AuthBridgeTest, ConnectingAfterAReauthRequiredStartsWithFullVerification) {
    Fixture f;
    storeSession(f.sessionPath(), *f.secrets);
    f.transport->queueResult(jsonResult(401, "{}"));
    f.transport->queueResult(jsonResult(401, R"({"detail":"Invalid refresh token"})"));

    QSignalSpy reauth(f.bridge.get(), &AuthBridge::reauthRequired);
    f.bridge->restoreSession();
    ASSERT_EQ(reauth.count(), 1);

    // ReauthRequired lässt die Sitzung stehen; der Nutzer gibt daraufhin eine
    // Adresse ein, und die Bridge baut ihre Verbindung neu auf.
    f.transport->queueResult(jsonResult(200, R"({"status":"ok"})"));
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));

    QSignalSpy ready(f.bridge.get(), &AuthBridge::pairingCodeReady);
    f.bridge->connectToServer(QStringLiteral("nas.example.com"));

    EXPECT_EQ(ready.count(), 1);
    ASSERT_FALSE(f.transport->calls.empty());
    EXPECT_EQ(f.transport->calls.back().policy.mode,
              baludesk::core::TlsPolicy::Mode::SystemTrust);
}
