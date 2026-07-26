#include "core/api/HttpClient.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using baludesk::core::HttpClient;
using baludesk::core::HttpErrorKind;
using baludesk::core::HttpMethod;
using baludesk::core::HttpRequest;
using baludesk::core::TlsPolicy;
using baludesk::core::Transport;
using baludesk::core::TransportResult;

namespace {

// Merkt sich, womit er aufgerufen wurde, und liefert eine vorgegebene Antwort.
class FakeTransport : public Transport {
public:
    struct Call {
        std::string url;
        HttpRequest request;
        TlsPolicy policy;
    };

    std::vector<Call> calls;
    TransportResult nextResult;

    TransportResult send(const std::string& url,
                         const HttpRequest& request,
                         const TlsPolicy& policy) override {
        calls.push_back(Call{url, request, policy});
        return nextResult;
    }
};

TransportResult okWith(long statusCode, std::string body) {
    TransportResult result;
    result.error = HttpErrorKind::None;
    result.response.statusCode = statusCode;
    result.response.body = std::move(body);
    return result;
}

TlsPolicy systemTrust() {
    TlsPolicy policy;
    policy.mode = TlsPolicy::Mode::SystemTrust;
    return policy;
}

}  // namespace

TEST(HttpClientTest, JoinsBaseUrlAndPathWithASingleSlash) {
    auto transport = std::make_shared<FakeTransport>();
    transport->nextResult = okWith(200, "{}");

    HttpClient client("https://nas.example.com", transport, systemTrust());
    client.get("/api/health");

    ASSERT_EQ(transport->calls.size(), std::size_t{1});
    EXPECT_EQ(transport->calls[0].url, "https://nas.example.com/api/health");
}

TEST(HttpClientTest, TolerationOfTrailingAndMissingSlashes) {
    auto transport = std::make_shared<FakeTransport>();
    transport->nextResult = okWith(200, "{}");

    HttpClient withSlash("https://nas.example.com/", transport, systemTrust());
    withSlash.get("/api/health");
    HttpClient withoutLeading("https://nas.example.com/", transport, systemTrust());
    withoutLeading.get("api/health");

    ASSERT_EQ(transport->calls.size(), std::size_t{2});
    EXPECT_EQ(transport->calls[0].url, "https://nas.example.com/api/health");
    EXPECT_EQ(transport->calls[1].url, "https://nas.example.com/api/health");
}

TEST(HttpClientTest, PassesTheBearerTokenThrough) {
    auto transport = std::make_shared<FakeTransport>();
    transport->nextResult = okWith(200, "{}");

    HttpClient client("https://nas.example.com", transport, systemTrust());
    client.get("/api/auth/me", "das-token");

    ASSERT_EQ(transport->calls.size(), std::size_t{1});
    EXPECT_EQ(transport->calls[0].request.bearerToken, "das-token");
    EXPECT_EQ(transport->calls[0].request.method, HttpMethod::Get);
}

TEST(HttpClientTest, SendsPostBodiesUnchanged) {
    auto transport = std::make_shared<FakeTransport>();
    transport->nextResult = okWith(201, "{}");

    HttpClient client("https://nas.example.com", transport, systemTrust());
    client.postJson("/api/desktop-pairing/poll", R"({"device_code":"abc"})");

    ASSERT_EQ(transport->calls.size(), std::size_t{1});
    EXPECT_EQ(transport->calls[0].request.method, HttpMethod::Post);
    EXPECT_EQ(transport->calls[0].request.jsonBody, R"({"device_code":"abc"})");
}

TEST(HttpClientTest, HandsTheTlsPolicyToTheTransport) {
    auto transport = std::make_shared<FakeTransport>();
    transport->nextResult = okWith(200, "{}");

    TlsPolicy pinned;
    pinned.mode = TlsPolicy::Mode::Pinned;
    pinned.publicKeyPin = "sha256//AAAA";

    HttpClient client("https://nas.example.com", transport, pinned);
    client.get("/api/health");

    ASSERT_EQ(transport->calls.size(), std::size_t{1});
    EXPECT_EQ(transport->calls[0].policy.mode, TlsPolicy::Mode::Pinned);
    EXPECT_EQ(transport->calls[0].policy.publicKeyPin, "sha256//AAAA");
}

TEST(HttpClientTest, AppliesAChangedTlsPolicyToLaterCalls) {
    auto transport = std::make_shared<FakeTransport>();
    transport->nextResult = okWith(200, "{}");

    HttpClient client("https://nas.example.com", transport, systemTrust());
    client.get("/api/health");

    TlsPolicy pinned;
    pinned.mode = TlsPolicy::Mode::Pinned;
    pinned.publicKeyPin = "sha256//BBBB";
    client.setTlsPolicy(pinned);
    client.get("/api/health");

    ASSERT_EQ(transport->calls.size(), std::size_t{2});
    EXPECT_EQ(transport->calls[0].policy.mode, TlsPolicy::Mode::SystemTrust);
    EXPECT_EQ(transport->calls[1].policy.mode, TlsPolicy::Mode::Pinned);
}

TEST(HttpClientTest, TreatsTwoHundredAsSuccess) {
    auto transport = std::make_shared<FakeTransport>();
    transport->nextResult = okWith(200, R"({"status":"ok"})");

    HttpClient client("https://nas.example.com", transport, systemTrust());
    const auto result = client.get("/api/health");

    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.statusCode, 200);
    EXPECT_EQ(result.body, R"({"status":"ok"})");
    EXPECT_EQ(result.error, HttpErrorKind::None);
}

TEST(HttpClientTest, DoesNotTreatFourHundredAsSuccess) {
    auto transport = std::make_shared<FakeTransport>();
    transport->nextResult = okWith(401, R"({"detail":"Not authenticated"})");

    HttpClient client("https://nas.example.com", transport, systemTrust());
    const auto result = client.get("/api/auth/me");

    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.statusCode, 401);
    // Der Rumpf bleibt erhalten - die Fehlermeldung des Servers ist die
    // brauchbarste Diagnose, die es gibt.
    EXPECT_EQ(result.body, R"({"detail":"Not authenticated"})");
}

TEST(HttpClientTest, CarriesTransportErrorsOutward) {
    auto transport = std::make_shared<FakeTransport>();
    TransportResult failure;
    failure.error = HttpErrorKind::CertificateUntrusted;
    failure.detail = "self signed certificate";
    failure.peerCertificatePem = "-----BEGIN CERTIFICATE-----\nXYZ\n-----END CERTIFICATE-----\n";
    transport->nextResult = failure;

    HttpClient client("https://nas.example.com", transport, systemTrust());
    const auto result = client.get("/api/health");

    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error, HttpErrorKind::CertificateUntrusted);
    EXPECT_EQ(result.detail, "self signed certificate");
    // Das Zertifikat der Gegenstelle muss durchgereicht werden - ohne es kann
    // der Nutzer den Fingerabdruck nicht bestätigen.
    EXPECT_FALSE(result.peerCertificatePem.empty());
}

TEST(HttpClientTest, ReportsAPinMismatchDistinctly) {
    auto transport = std::make_shared<FakeTransport>();
    TransportResult failure;
    failure.error = HttpErrorKind::CertificatePinMismatch;
    failure.detail = "pinned public key mismatch";
    transport->nextResult = failure;

    HttpClient client("https://nas.example.com", transport, systemTrust());
    const auto result = client.get("/api/health");

    // Eigene Fehlerart, weil daraus etwas anderes folgt als aus einem
    // unbekannten Zertifikat: hier wird nicht nachgefragt, sondern hart
    // abgebrochen.
    EXPECT_EQ(result.error, HttpErrorKind::CertificatePinMismatch);
}

TEST(HttpClientTest, ExposesItsBaseUrl) {
    auto transport = std::make_shared<FakeTransport>();
    HttpClient client("https://nas.example.com/", transport, systemTrust());
    EXPECT_EQ(client.baseUrl(), "https://nas.example.com");
}
