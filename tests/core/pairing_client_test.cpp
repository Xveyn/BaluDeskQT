#include "core/auth/PairingClient.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "core/api/HttpClient.h"
#include "support/FakeTransport.h"

using baludesk::core::HttpClient;
using baludesk::core::HttpErrorKind;
using baludesk::core::PairingClient;
using baludesk::core::PairingState;
using baludesk::core::TlsPolicy;
using baludesk::core::TransportResult;
using baludesk::core::test::FakeTransport;

namespace {

TransportResult jsonResult(long statusCode, std::string body) {
    TransportResult result;
    result.error = HttpErrorKind::None;
    result.response.statusCode = statusCode;
    result.response.body = std::move(body);
    return result;
}

TransportResult transportFailure(HttpErrorKind kind, std::string detail) {
    TransportResult result;
    result.error = kind;
    result.detail = std::move(detail);
    return result;
}

const char* kDeviceCodeResponse = R"({
  "device_code": "geheimer-geraetecode",
  "user_code": "483920",
  "verification_url": "https://nas.example.com/devices?pair=1",
  "expires_in": 600,
  "interval": 5
})";

const char* kPendingResponse = R"({"status":"authorization_pending"})";

const char* kApprovedResponse = R"({
  "status": "approved",
  "access_token": "zugangstoken",
  "refresh_token": "erneuerungstoken",
  "token_type": "bearer",
  "user": {"id": 1, "username": "sven", "email": "sven@example.com", "role": "admin"}
})";

struct Fixture {
    std::shared_ptr<FakeTransport> transport = std::make_shared<FakeTransport>();
    std::unique_ptr<HttpClient> http;
    std::unique_ptr<PairingClient> client;

    Fixture() {
        http = std::make_unique<HttpClient>("https://nas.example.com", transport, TlsPolicy{});
        client = std::make_unique<PairingClient>(*http);
    }
};

}  // namespace

TEST(PairingClientTest, StartsIdle) {
    Fixture f;
    EXPECT_EQ(f.client->state(), PairingState::Idle);
    EXPECT_FALSE(f.client->prompt().has_value());
    EXPECT_FALSE(f.client->tokens().has_value());
}

TEST(PairingClientTest, BeginRequestsACodeAndExposesThePrompt) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));

    const auto progress = f.client->begin("geraet-1", "Svens Rechner", 1000);

    EXPECT_EQ(progress.state, PairingState::WaitingForApproval);
    ASSERT_TRUE(f.client->prompt().has_value());
    EXPECT_EQ(f.client->prompt()->userCode, "483920");
    EXPECT_EQ(f.client->prompt()->verificationUrl, "https://nas.example.com/devices?pair=1");
    EXPECT_EQ(f.client->prompt()->expiresInSeconds, 600);
}

TEST(PairingClientTest, BeginSendsTheExpectedRequest) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));

    f.client->begin("geraet-1", "Svens Rechner", 1000);

    ASSERT_EQ(f.transport->calls.size(), std::size_t{1});
    const auto& call = f.transport->calls[0];
    EXPECT_EQ(call.url, "https://nas.example.com/api/desktop-pairing/device-code");
    EXPECT_NE(call.request.jsonBody.find("\"device_id\":\"geraet-1\""), std::string::npos);
    EXPECT_NE(call.request.jsonBody.find("\"device_name\":\"Svens Rechner\""), std::string::npos);
    // Der Server prüft platform gegen ^(windows|mac|linux)$.
    EXPECT_NE(call.request.jsonBody.find("\"platform\":\"windows\""), std::string::npos);
    // Pairing läuft ohne Anmeldung.
    EXPECT_TRUE(call.request.bearerToken.empty());
}

TEST(PairingClientTest, HonoursTheIntervalTheServerAsksFor) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));

    const auto progress = f.client->begin("geraet-1", "Rechner", 1000);

    // Der Server gibt interval=5 vor, und bei 12 Anfragen je Minute ist das
    // exakt ein Abruf je Intervall - ohne jede Reserve. Schneller zu fragen
    // sperrt den Client aus.
    EXPECT_EQ(progress.waitSeconds, 5);
}

TEST(PairingClientTest, StaysWaitingWhileTheServerReportsPending) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.client->begin("geraet-1", "Rechner", 1000);

    f.transport->queueResult(jsonResult(200, kPendingResponse));
    const auto progress = f.client->poll(1005);

    EXPECT_EQ(progress.state, PairingState::WaitingForApproval);
    EXPECT_EQ(progress.waitSeconds, 5);
    EXPECT_FALSE(f.client->tokens().has_value());
}

TEST(PairingClientTest, PollSendsTheDeviceCodeBack) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.client->begin("geraet-1", "Rechner", 1000);

    f.transport->queueResult(jsonResult(200, kPendingResponse));
    f.client->poll(1005);

    ASSERT_EQ(f.transport->calls.size(), std::size_t{2});
    const auto& call = f.transport->calls[1];
    EXPECT_EQ(call.url, "https://nas.example.com/api/desktop-pairing/poll");
    EXPECT_NE(call.request.jsonBody.find("geheimer-geraetecode"), std::string::npos);
}

TEST(PairingClientTest, CapturesTheTokensOnApproval) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.client->begin("geraet-1", "Rechner", 1000);

    f.transport->queueResult(jsonResult(200, kApprovedResponse));
    const auto progress = f.client->poll(1005);

    EXPECT_EQ(progress.state, PairingState::Approved);
    ASSERT_TRUE(f.client->tokens().has_value());
    EXPECT_EQ(f.client->tokens()->accessToken, "zugangstoken");
    EXPECT_EQ(f.client->tokens()->refreshToken, "erneuerungstoken");
    EXPECT_EQ(f.client->tokens()->username, "sven");
}

TEST(PairingClientTest, DoesNotPollAgainAfterApproval) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.client->begin("geraet-1", "Rechner", 1000);
    f.transport->queueResult(jsonResult(200, kApprovedResponse));
    f.client->poll(1005);

    const std::size_t callsAfterApproval = f.transport->calls.size();
    const auto progress = f.client->poll(1010);

    // Der Server löscht den Datensatz bei der Auslieferung. Ein zweiter
    // Abruf brachte nur ein 404 und würde den erreichten Zustand zerstören.
    EXPECT_EQ(progress.state, PairingState::Approved);
    EXPECT_EQ(f.transport->calls.size(), callsAfterApproval);
    ASSERT_TRUE(f.client->tokens().has_value());
    EXPECT_EQ(f.client->tokens()->accessToken, "zugangstoken");
}

TEST(PairingClientTest, ReportsDenial) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.client->begin("geraet-1", "Rechner", 1000);

    f.transport->queueResult(jsonResult(200, R"({"status":"denied"})"));
    const auto progress = f.client->poll(1005);

    EXPECT_EQ(progress.state, PairingState::Denied);
    EXPECT_FALSE(progress.message.empty());
    EXPECT_FALSE(f.client->tokens().has_value());
}

TEST(PairingClientTest, ReportsExpiryFromTheServer) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.client->begin("geraet-1", "Rechner", 1000);

    f.transport->queueResult(jsonResult(200, R"({"status":"expired"})"));
    const auto progress = f.client->poll(1005);

    EXPECT_EQ(progress.state, PairingState::Expired);
}

TEST(PairingClientTest, StopsOnItsOwnWhenTheCodeLifetimeRunsOut) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.client->begin("geraet-1", "Rechner", 1000);

    // expires_in war 600. Nach 601 Sekunden ist nichts mehr zu holen -
    // weiterzufragen verbraucht nur Kontingent.
    const std::size_t callsBefore = f.transport->calls.size();
    const auto progress = f.client->poll(1601);

    EXPECT_EQ(progress.state, PairingState::Expired);
    EXPECT_EQ(f.transport->calls.size(), callsBefore);
}

TEST(PairingClientTest, TreatsAMissingPairingRecordAsExpired) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.client->begin("geraet-1", "Rechner", 1000);

    // 404 "Invalid device code" - der Datensatz ist weg. Für den Nutzer ist
    // das dasselbe wie abgelaufen.
    f.transport->queueResult(jsonResult(404, R"({"detail":"Invalid device code"})"));
    const auto progress = f.client->poll(1005);

    EXPECT_EQ(progress.state, PairingState::Expired);
}

TEST(PairingClientTest, BacksOffWhenTheServerReportsTooManyRequests) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.client->begin("geraet-1", "Rechner", 1000);

    f.transport->queueResult(jsonResult(
        429, R"({"error":"Too Many Requests","detail":"Rate limit exceeded.","retry_after":37})"));
    const auto progress = f.client->poll(1005);

    // Der Ablauf bricht nicht ab, er wartet länger - und zwar so lange, wie
    // der Server im Rumpf angibt. Kopfzeilen trägt HttpResponse nicht.
    EXPECT_EQ(progress.state, PairingState::WaitingForApproval);
    EXPECT_EQ(progress.waitSeconds, 37);
}

TEST(PairingClientTest, FallsBackToASaneDelayWhenRetryAfterIsMissing) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.client->begin("geraet-1", "Rechner", 1000);

    f.transport->queueResult(jsonResult(429, R"({"error":"Too Many Requests"})"));
    const auto progress = f.client->poll(1005);

    EXPECT_EQ(progress.state, PairingState::WaitingForApproval);
    EXPECT_GE(progress.waitSeconds, 30);
}

TEST(PairingClientTest, KeepsWaitingThroughATransientNetworkError) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.client->begin("geraet-1", "Rechner", 1000);

    f.transport->queueResult(transportFailure(HttpErrorKind::Network, "connection refused"));
    const auto progress = f.client->poll(1005);

    // Ein schlafender oder gerade neu startender NAS ist kein Grund, die
    // Paarung wegzuwerfen - der Code lebt zehn Minuten.
    EXPECT_EQ(progress.state, PairingState::WaitingForApproval);
    EXPECT_GT(progress.waitSeconds, 0);
}

TEST(PairingClientTest, GivesUpOnAPinMismatch) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.client->begin("geraet-1", "Rechner", 1000);

    f.transport->queueResult(
        transportFailure(HttpErrorKind::CertificatePinMismatch, "pinned key mismatch"));
    const auto progress = f.client->poll(1005);

    // Spec Abschnitt 2.9: bei einer Pin-Abweichung wird nicht nachgefragt und
    // nicht weitergemacht, sondern hart abgebrochen.
    EXPECT_EQ(progress.state, PairingState::Failed);
    EXPECT_FALSE(progress.message.empty());
}

TEST(PairingClientTest, FailsWhenTheServerRefusesToIssueACode) {
    Fixture f;
    f.transport->queueResult(jsonResult(500, R"({"detail":"boom"})"));

    const auto progress = f.client->begin("geraet-1", "Rechner", 1000);

    EXPECT_EQ(progress.state, PairingState::Failed);
    EXPECT_FALSE(f.client->prompt().has_value());
}

TEST(PairingClientTest, FailsOnAnUnreadableCodeResponse) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, "kein json"));

    const auto progress = f.client->begin("geraet-1", "Rechner", 1000);

    EXPECT_EQ(progress.state, PairingState::Failed);
}

TEST(PairingClientTest, FailsOnAnApprovalWithoutTokens) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.client->begin("geraet-1", "Rechner", 1000);

    // Freigegeben, aber ohne Token - damit ist nichts anzufangen, und weil
    // der Server den Datensatz gelöscht hat, hilft auch kein zweiter Versuch.
    f.transport->queueResult(jsonResult(200, R"({"status":"approved"})"));
    const auto progress = f.client->poll(1005);

    EXPECT_EQ(progress.state, PairingState::Failed);
    EXPECT_FALSE(f.client->tokens().has_value());
}

TEST(PairingClientTest, PollBeforeBeginIsRefusedWithoutTouchingTheNetwork) {
    Fixture f;

    const auto progress = f.client->poll(1000);

    EXPECT_EQ(progress.state, PairingState::Idle);
    EXPECT_TRUE(f.transport->calls.empty());
}

TEST(PairingClientTest, NeverRetriesFasterThanTheServerAskedFor) {
    Fixture f;
    // Server gibt hier 30 Sekunden vor - deutlich mehr als der feste
    // Rückfallwert bei Netzfehlern.
    f.transport->queueResult(jsonResult(200, R"({
      "device_code": "geheimer-geraetecode",
      "user_code": "483920",
      "verification_url": "https://nas.example.com/devices?pair=1",
      "expires_in": 600,
      "interval": 30
    })"));
    f.client->begin("geraet-1", "Rechner", 1000);

    f.transport->queueResult(transportFailure(HttpErrorKind::Network, "connection refused"));
    const auto progress = f.client->poll(1030);

    // Mit einem festen Rückfallwert käme hier 10 heraus - und der Client
    // würde dreimal so schnell fragen, wie der Server erlaubt.
    EXPECT_EQ(progress.state, PairingState::WaitingForApproval);
    EXPECT_GE(progress.waitSeconds, 30);
}
