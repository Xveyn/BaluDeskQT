#include "bridge/AuthBridge.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QFile>
#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>

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
