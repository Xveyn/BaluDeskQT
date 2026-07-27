#include "core/api/TrustNegotiation.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "core/api/HttpClient.h"
#include "support/FakeTransport.h"

using baludesk::core::HttpClient;
using baludesk::core::HttpErrorKind;
using baludesk::core::TlsPolicy;
using baludesk::core::TransportResult;
using baludesk::core::TrustNegotiation;
using baludesk::core::TrustState;
using baludesk::core::test::FakeTransport;

namespace {

TransportResult okResult() {
    TransportResult result;
    result.error = HttpErrorKind::None;
    result.response.statusCode = 200;
    result.response.body = R"({"status":"ok"})";
    return result;
}

TransportResult statusResult(long statusCode) {
    TransportResult result;
    result.error = HttpErrorKind::None;
    result.response.statusCode = statusCode;
    return result;
}

TransportResult failure(HttpErrorKind kind, std::string detail) {
    TransportResult result;
    result.error = kind;
    result.detail = std::move(detail);
    return result;
}

// Ein echtes Zertifikat, damit identifyCertificate() etwas zu tun bekommt -
// derselbe Weg wie in tls_trust_test.cpp. Der Pfad kommt beim Übersetzen
// herein, damit der Test unabhängig vom Arbeitsverzeichnis läuft.
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

struct Fixture {
    std::shared_ptr<FakeTransport> transport = std::make_shared<FakeTransport>();
    std::unique_ptr<HttpClient> http;
    std::unique_ptr<TrustNegotiation> trust;

    Fixture() {
        http = std::make_unique<HttpClient>("https://nas.example.com", transport, TlsPolicy{});
        trust = std::make_unique<TrustNegotiation>(*http);
    }
};

}  // namespace

TEST(TrustNegotiationTest, StartsIdle) {
    Fixture f;
    EXPECT_EQ(f.trust->state(), TrustState::Idle);
    EXPECT_FALSE(f.trust->prompt().has_value());
    EXPECT_TRUE(f.trust->confirmedPin().empty());
}

TEST(TrustNegotiationTest, ReportsTrustedWhenTheCertificateChecksOut) {
    Fixture f;
    f.transport->queueResult(okResult());

    EXPECT_EQ(f.trust->probe(), TrustState::Trusted);
    EXPECT_FALSE(f.trust->prompt().has_value());
    // Kein Pin: bei regulärer Prüfung gibt es nichts anzuheften, und ein
    // trotzdem gesetzter Wert würde die Sitzung grundlos an ein Zertifikat
    // binden, das sich jederzeit erneuern darf.
    EXPECT_TRUE(f.trust->confirmedPin().empty());

    ASSERT_EQ(f.transport->calls.size(), std::size_t{1});
    EXPECT_EQ(f.transport->calls[0].url, "https://nas.example.com/api/health");
}

TEST(TrustNegotiationTest, AsksForConfirmationOnAnUnknownCertificate) {
    Fixture f;
    f.transport->queueResult(untrustedResult(testCertificate()));

    EXPECT_EQ(f.trust->probe(), TrustState::ConfirmationRequired);
    ASSERT_TRUE(f.trust->prompt().has_value());
    EXPECT_FALSE(f.trust->prompt()->fingerprintSha256.empty());
    EXPECT_FALSE(f.trust->prompt()->publicKeyPin.empty());
    EXPECT_EQ(f.trust->prompt()->reason, "self signed certificate");
    // Erst nach confirm() gilt der Pin als bestätigt.
    EXPECT_TRUE(f.trust->confirmedPin().empty());
}

TEST(TrustNegotiationTest, ReportsAPinMismatchWithoutAsking) {
    Fixture f;
    f.transport->queueResult(failure(HttpErrorKind::CertificatePinMismatch, "pinned key mismatch"));

    // Spec 2.9: bei einer Pin-Abweichung wird nicht nachgefragt.
    EXPECT_EQ(f.trust->probe(), TrustState::PinMismatch);
    EXPECT_FALSE(f.trust->prompt().has_value());
    EXPECT_FALSE(f.trust->message().empty());
}

TEST(TrustNegotiationTest, TreatsANetworkFailureAsUnreachable) {
    Fixture f;
    f.transport->queueResult(failure(HttpErrorKind::Network, "connection refused"));

    // Ein schlafender NAS ist laut Spec Abschnitt 10 Betriebszustand, nicht
    // Fehler - und ganz sicher keine Aussage über sein Zertifikat.
    EXPECT_EQ(f.trust->probe(), TrustState::Unreachable);
    EXPECT_FALSE(f.trust->message().empty());
}

TEST(TrustNegotiationTest, TreatsATimeoutAsUnreachable) {
    Fixture f;
    f.transport->queueResult(failure(HttpErrorKind::Timeout, "timed out"));

    EXPECT_EQ(f.trust->probe(), TrustState::Unreachable);
}

TEST(TrustNegotiationTest, FailsWhenNoCertificateCouldBeHarvested) {
    Fixture f;
    f.transport->queueResult(untrustedResult(""));

    // Ohne Zertifikat gibt es keinen Fingerabdruck zu zeigen. Eine Nachfrage
    // ohne Vergleichsmöglichkeit wäre eine Klickübung, kein Sicherheitsschritt.
    EXPECT_EQ(f.trust->probe(), TrustState::Failed);
    EXPECT_FALSE(f.trust->prompt().has_value());
    EXPECT_FALSE(f.trust->message().empty());
}

TEST(TrustNegotiationTest, FailsOnAnUnreadableCertificate) {
    Fixture f;
    f.transport->queueResult(untrustedResult("das ist kein PEM"));

    EXPECT_EQ(f.trust->probe(), TrustState::Failed);
    EXPECT_FALSE(f.trust->prompt().has_value());
}

TEST(TrustNegotiationTest, FailsWhenTheServerAnswersWithAnUnexpectedStatus) {
    Fixture f;
    // 404 auf /api/health: die Verbindung steht, aber am anderen Ende ist
    // kein BaluHost. Weiterzupaaren würde später an unverständlicher Stelle
    // scheitern.
    f.transport->queueResult(statusResult(404));

    EXPECT_EQ(f.trust->probe(), TrustState::Failed);
    EXPECT_FALSE(f.trust->message().empty());
}

TEST(TrustNegotiationTest, ConfirmPinsTheKeyAndChecksAgain) {
    Fixture f;
    f.transport->queueResult(untrustedResult(testCertificate()));
    ASSERT_EQ(f.trust->probe(), TrustState::ConfirmationRequired);
    const std::string offeredPin = f.trust->prompt()->publicKeyPin;

    f.transport->queueResult(okResult());
    EXPECT_EQ(f.trust->confirm(), TrustState::Trusted);

    EXPECT_EQ(f.trust->confirmedPin(), offeredPin);
    ASSERT_EQ(f.transport->calls.size(), std::size_t{2});
    // Der zweite Aufruf muss die angeheftete Regel tragen - sonst wäre die
    // Bestätigung folgenlos geblieben.
    EXPECT_EQ(f.transport->calls[1].policy.mode, TlsPolicy::Mode::Pinned);
    EXPECT_EQ(f.transport->calls[1].policy.publicKeyPin, offeredPin);
}

TEST(TrustNegotiationTest, ConfirmWithoutAPromptTouchesNothing) {
    Fixture f;

    // Nichts zu bestätigen. Der Aufruf darf weder ins Netz greifen noch
    // irgendetwas anheften.
    EXPECT_EQ(f.trust->confirm(), TrustState::Idle);
    EXPECT_TRUE(f.transport->calls.empty());
    EXPECT_TRUE(f.trust->confirmedPin().empty());
}

TEST(TrustNegotiationTest, ConfirmKeepsNoPinWhenTheCheckStillFails) {
    Fixture f;
    f.transport->queueResult(untrustedResult(testCertificate()));
    ASSERT_EQ(f.trust->probe(), TrustState::ConfirmationRequired);

    f.transport->queueResult(failure(HttpErrorKind::Network, "connection reset"));
    EXPECT_EQ(f.trust->confirm(), TrustState::Unreachable);

    // Ein Pin, der nie zu einer stehenden Verbindung geführt hat, gehört
    // nicht in die Sitzung - er würde dort dauerhaft eine Abweichung
    // erzeugen, die niemand mehr zuordnen kann.
    EXPECT_TRUE(f.trust->confirmedPin().empty());
}

TEST(TrustNegotiationTest, ConfirmReleasesThePinnedPolicyWhenTheCheckFails) {
    Fixture f;
    f.transport->queueResult(untrustedResult(testCertificate()));
    ASSERT_EQ(f.trust->probe(), TrustState::ConfirmationRequired);

    f.transport->queueResult(failure(HttpErrorKind::Network, "connection reset"));
    ASSERT_EQ(f.trust->confirm(), TrustState::Unreachable);

    // Der HttpClient gehört der Klasse nicht und überlebt sie. Bliebe er auf
    // einem nie bestätigten Schlüssel stehen, wäre jede spätere Prüfung über
    // dieses Paar gegen ein Zertifikat gerichtet, dem nie jemand zugestimmt hat.
    f.transport->queueResult(okResult());
    ASSERT_EQ(f.trust->probe(), TrustState::Trusted);

    const auto& lastCall = f.transport->calls.back();
    EXPECT_EQ(lastCall.policy.mode, TlsPolicy::Mode::SystemTrust);
    EXPECT_TRUE(lastCall.policy.publicKeyPin.empty());
}

TEST(TrustNegotiationTest, ForgetsThePromptWhenTheServerBecomesUnreachable) {
    Fixture f;
    f.transport->queueResult(untrustedResult(testCertificate()));
    ASSERT_EQ(f.trust->probe(), TrustState::ConfirmationRequired);
    ASSERT_TRUE(f.trust->prompt().has_value());

    f.transport->queueResult(failure(HttpErrorKind::Network, "connection reset"));
    ASSERT_EQ(f.trust->confirm(), TrustState::Unreachable);

    // Ein belegtes prompt_ gehört ausschliesslich zu ConfirmationRequired.
    // Sonst zeigt der Dialog eine Zertifikatsnachfrage für einen Server, der
    // gerade nur nicht erreichbar ist.
    EXPECT_FALSE(f.trust->prompt().has_value());
}
