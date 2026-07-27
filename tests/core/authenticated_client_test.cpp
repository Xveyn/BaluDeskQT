#include "core/auth/AuthenticatedClient.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "core/api/HttpClient.h"
#include "support/FakeSessionRepository.h"
#include "support/FakeTransport.h"

using baludesk::core::AuthenticatedClient;
using baludesk::core::AuthOutcome;
using baludesk::core::HttpClient;
using baludesk::core::HttpErrorKind;
using baludesk::core::Session;
using baludesk::core::TlsPolicy;
using baludesk::core::TransportResult;
using baludesk::core::test::FakeSessionRepository;
using baludesk::core::test::FakeTransport;

namespace {

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

Session exampleSession() {
    Session session;
    session.serverUrl = "https://nas.example.com";
    session.username = "sven";
    session.publicKeyPin.clear();
    session.accessToken = "altes-zugangstoken";
    session.refreshToken = "erneuerungstoken";
    return session;
}

const char* kRefreshResponse = R"({
  "access_token": "neues-zugangstoken",
  "token_type": "bearer",
  "user": {"id": 1, "username": "sven"}
})";

struct Fixture {
    std::shared_ptr<FakeTransport> transport = std::make_shared<FakeTransport>();
    FakeSessionRepository repository;
    std::unique_ptr<HttpClient> http;
    std::unique_ptr<AuthenticatedClient> client;

    Fixture() {
        repository.stored = exampleSession();
        http = std::make_unique<HttpClient>("https://nas.example.com", transport, TlsPolicy{});
        client = std::make_unique<AuthenticatedClient>(*http, repository, exampleSession());
    }
};

}  // namespace

TEST(AuthenticatedClientTest, PassesASuccessfulCallThroughUntouched) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, R"({"username":"sven"})"));

    const auto result = f.client->get("/api/auth/me");

    EXPECT_EQ(result.outcome, AuthOutcome::Ok);
    EXPECT_TRUE(result.usable());
    EXPECT_EQ(result.response.statusCode, 200);
    EXPECT_EQ(f.transport->calls.size(), std::size_t{1});
    EXPECT_EQ(f.repository.saveAccessTokenCallCount, std::size_t{0});
}

TEST(AuthenticatedClientTest, SendsTheAccessTokenAsBearer) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, "{}"));

    f.client->get("/api/auth/me");

    ASSERT_EQ(f.transport->calls.size(), std::size_t{1});
    EXPECT_EQ(f.transport->calls[0].request.bearerToken, "altes-zugangstoken");
}

TEST(AuthenticatedClientTest, RefreshesOnceOnA401AndRepeatsTheCall) {
    Fixture f;
    f.transport->queueResult(jsonResult(401, R"({"detail":"Not authenticated"})"));
    f.transport->queueResult(jsonResult(200, kRefreshResponse));
    f.transport->queueResult(jsonResult(200, R"({"username":"sven"})"));

    const auto result = f.client->get("/api/auth/me");

    EXPECT_EQ(result.outcome, AuthOutcome::Refreshed);
    EXPECT_TRUE(result.usable());
    EXPECT_EQ(result.response.statusCode, 200);

    ASSERT_EQ(f.transport->calls.size(), std::size_t{3});
    EXPECT_EQ(f.transport->calls[1].url, "https://nas.example.com/api/auth/refresh");
    // Die Erneuerung geht ohne Bearer und mit dem Erneuerungstoken im Rumpf.
    EXPECT_TRUE(f.transport->calls[1].request.bearerToken.empty());
    EXPECT_NE(f.transport->calls[1].request.jsonBody.find("erneuerungstoken"),
              std::string::npos);
    // Die Wiederholung trägt das neue Zugangstoken, nicht das alte.
    EXPECT_EQ(f.transport->calls[2].request.bearerToken, "neues-zugangstoken");
}

TEST(AuthenticatedClientTest, StoresTheRefreshedAccessToken) {
    Fixture f;
    f.transport->queueResult(jsonResult(401, "{}"));
    f.transport->queueResult(jsonResult(200, kRefreshResponse));
    f.transport->queueResult(jsonResult(200, "{}"));

    f.client->get("/api/auth/me");

    EXPECT_EQ(f.repository.saveAccessTokenCallCount, std::size_t{1});
    ASSERT_TRUE(f.repository.stored.has_value());
    EXPECT_EQ(f.repository.stored->accessToken, "neues-zugangstoken");
    // Das Erneuerungstoken wird vom Server nicht rotiert.
    EXPECT_EQ(f.repository.stored->refreshToken, "erneuerungstoken");
    // Die Datei wird dabei nicht angefasst.
    EXPECT_EQ(f.repository.saveCallCount, std::size_t{0});
    EXPECT_EQ(f.client->session().accessToken, "neues-zugangstoken");
}

TEST(AuthenticatedClientTest, DoesNotRefreshTwiceForOneCall) {
    Fixture f;
    f.transport->queueResult(jsonResult(401, "{}"));
    f.transport->queueResult(jsonResult(200, kRefreshResponse));
    f.transport->queueResult(jsonResult(401, "{}"));

    const auto result = f.client->get("/api/auth/me");

    // Genau ein Erneuerungsversuch je Aufruf. Ohne diese Grenze dreht sich
    // der Client im Kreis: 401 -> erneuern -> 401 -> erneuern -> ...
    ASSERT_EQ(f.transport->calls.size(), std::size_t{3});
    EXPECT_EQ(f.repository.saveAccessTokenCallCount, std::size_t{1});
    // Der Aufruf ist durch, das Ergebnis ist ein 401 - der Aufrufer sieht die
    // rohe Wahrheit statt einer Deutung.
    EXPECT_EQ(result.outcome, AuthOutcome::Ok);
    EXPECT_EQ(result.response.statusCode, 401);
}

TEST(AuthenticatedClientTest, ReportsReauthRequiredWhenTheRefreshTokenIsRejected) {
    Fixture f;
    f.transport->queueResult(jsonResult(401, "{}"));
    f.transport->queueResult(jsonResult(401, R"({"detail":"Invalid refresh token"})"));

    const auto result = f.client->get("/api/auth/me");

    // Das Erneuerungstoken ist tot. Kein Erneuern hilft mehr - nur Neupaaren.
    // Genau diese Unterscheidung von Unreachable verhindert die Schleife.
    EXPECT_EQ(result.outcome, AuthOutcome::ReauthRequired);
    EXPECT_FALSE(result.usable());
    EXPECT_EQ(f.transport->calls.size(), std::size_t{2});
    EXPECT_EQ(f.repository.saveAccessTokenCallCount, std::size_t{0});
}

TEST(AuthenticatedClientTest, ReportsReauthRequiredWhenTheRefreshCarriesNoToken) {
    Fixture f;
    f.transport->queueResult(jsonResult(401, "{}"));
    f.transport->queueResult(jsonResult(200, R"({"token_type":"bearer"})"));

    const auto result = f.client->get("/api/auth/me");

    // 200, aber ohne access_token: damit ist nichts anzufangen, und ein
    // zweiter Versuch brächte dasselbe.
    EXPECT_EQ(result.outcome, AuthOutcome::ReauthRequired);
    EXPECT_EQ(f.client->session().accessToken, "altes-zugangstoken");
}

TEST(AuthenticatedClientTest, ReportsUnreachableWhenTheRefreshCannotBeSent) {
    Fixture f;
    f.transport->queueResult(jsonResult(401, "{}"));
    f.transport->queueResult(failure(HttpErrorKind::Network, "connection refused"));

    const auto result = f.client->get("/api/auth/me");

    // Der Server schläft. Die Sitzung ist unberührt und morgen wieder gut -
    // sie hier zu verwerfen wäre der teuerste denkbare Fehlschluss.
    EXPECT_EQ(result.outcome, AuthOutcome::Unreachable);
    EXPECT_EQ(f.client->session().accessToken, "altes-zugangstoken");
    EXPECT_TRUE(f.repository.stored.has_value());
}

TEST(AuthenticatedClientTest, ReportsUnreachableWhenTheServerErrorsDuringRefresh) {
    Fixture f;
    f.transport->queueResult(jsonResult(401, "{}"));
    f.transport->queueResult(jsonResult(500, R"({"detail":"boom"})"));

    const auto result = f.client->get("/api/auth/me");

    // 5xx ist ein Problem des Servers, keine Aussage über das Token.
    EXPECT_EQ(result.outcome, AuthOutcome::Unreachable);
}

TEST(AuthenticatedClientTest, ReportsTrustBrokenWithoutAttemptingARefresh) {
    Fixture f;
    f.transport->queueResult(failure(HttpErrorKind::CertificatePinMismatch, "pinned key mismatch"));

    const auto result = f.client->get("/api/auth/me");

    // Spec 2.9: harter Abbruch. Ein Erneuerungsversuch über dieselbe
    // Verbindung würde das Erneuerungstoken an eine Gegenstelle schicken,
    // der gerade nicht zu trauen ist.
    EXPECT_EQ(result.outcome, AuthOutcome::TrustBroken);
    EXPECT_EQ(f.transport->calls.size(), std::size_t{1});
}

TEST(AuthenticatedClientTest, LeavesOtherStatusCodesAlone) {
    Fixture f;
    f.transport->queueResult(jsonResult(403, R"({"detail":"Forbidden"})"));

    const auto result = f.client->get("/api/files/list");

    // 403 heisst „darfst du nicht", nicht „bist du nicht". Erneuern ändert
    // daran nichts.
    EXPECT_EQ(result.outcome, AuthOutcome::Ok);
    EXPECT_EQ(result.response.statusCode, 403);
    EXPECT_EQ(f.transport->calls.size(), std::size_t{1});
}

TEST(AuthenticatedClientTest, PostJsonTakesTheSamePath) {
    Fixture f;
    f.transport->queueResult(jsonResult(401, "{}"));
    f.transport->queueResult(jsonResult(200, kRefreshResponse));
    f.transport->queueResult(jsonResult(200, R"({"changes":[]})"));

    const auto result = f.client->postJson("/api/sync/changes", R"({"since":0})");

    EXPECT_EQ(result.outcome, AuthOutcome::Refreshed);
    ASSERT_EQ(f.transport->calls.size(), std::size_t{3});
    EXPECT_EQ(f.transport->calls[2].url, "https://nas.example.com/api/sync/changes");
    EXPECT_EQ(f.transport->calls[2].request.jsonBody, R"({"since":0})");
    EXPECT_EQ(f.transport->calls[2].request.bearerToken, "neues-zugangstoken");
}
