#include "core/util/TlsTrust.h"

#include <gtest/gtest.h>

#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using baludesk::core::identifyCertificate;

namespace {

// Der Pfad wird beim Übersetzen über eine Definition hereingereicht, damit die
// Tests unabhängig vom Arbeitsverzeichnis laufen.
std::string readTestData(const std::string& name) {
    const fs::path path = fs::path(BALUDESK_TEST_DATA_DIR) / name;
    std::ifstream stream(path, std::ios::binary);
    EXPECT_TRUE(stream.good()) << "Testdatei nicht lesbar: " << path.string();
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

bool isUpperHex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
}

}  // namespace

TEST(TlsTrustTest, IdentifiesAValidCertificate) {
    const auto identity = identifyCertificate(readTestData("test-cert-a.pem"));
    ASSERT_TRUE(identity.has_value());
    EXPECT_FALSE(identity->fingerprintSha256.empty());
    EXPECT_FALSE(identity->publicKeyPin.empty());
}

TEST(TlsTrustTest, FingerprintHasTheExpectedShape) {
    const auto identity = identifyCertificate(readTestData("test-cert-a.pem"));
    ASSERT_TRUE(identity.has_value());

    // 32 Bytes als Hexpaare mit Doppelpunkt dazwischen: 32*2 + 31 = 95 Zeichen.
    const std::string& fingerprint = identity->fingerprintSha256;
    ASSERT_EQ(fingerprint.size(), std::size_t{95});

    for (std::size_t i = 0; i < fingerprint.size(); ++i) {
        if ((i % 3) == 2) {
            EXPECT_EQ(fingerprint[i], ':') << "an Position " << i;
        } else {
            EXPECT_TRUE(isUpperHex(fingerprint[i]))
                << "an Position " << i << ": '" << fingerprint[i] << "'";
        }
    }
}

TEST(TlsTrustTest, PinUsesTheFormatCurlExpects) {
    const auto identity = identifyCertificate(readTestData("test-cert-a.pem"));
    ASSERT_TRUE(identity.has_value());

    const std::string& pin = identity->publicKeyPin;
    ASSERT_GT(pin.size(), std::size_t{8});
    EXPECT_EQ(pin.substr(0, 8), "sha256//");

    // SHA-256 sind 32 Bytes; Base64 davon ist 44 Zeichen mit einem '=' am Ende.
    const std::string encoded = pin.substr(8);
    EXPECT_EQ(encoded.size(), std::size_t{44});
    EXPECT_EQ(encoded.back(), '=');
}

TEST(TlsTrustTest, IsDeterministic) {
    const std::string pem = readTestData("test-cert-a.pem");
    const auto first = identifyCertificate(pem);
    const auto second = identifyCertificate(pem);

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(first->fingerprintSha256, second->fingerprintSha256);
    EXPECT_EQ(first->publicKeyPin, second->publicKeyPin);
}

TEST(TlsTrustTest, DistinguishesDifferentCertificates) {
    const auto a = identifyCertificate(readTestData("test-cert-a.pem"));
    const auto b = identifyCertificate(readTestData("test-cert-b.pem"));

    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_NE(a->fingerprintSha256, b->fingerprintSha256);
    EXPECT_NE(a->publicKeyPin, b->publicKeyPin);
}

TEST(TlsTrustTest, RejectsGarbageInsteadOfCrashing) {
    EXPECT_FALSE(identifyCertificate("").has_value());
    EXPECT_FALSE(identifyCertificate("kein Zertifikat, nur Text").has_value());
    EXPECT_FALSE(identifyCertificate("-----BEGIN CERTIFICATE-----\nnicht base64\n-----END CERTIFICATE-----\n")
                     .has_value());
}

TEST(TlsTrustTest, RejectsATruncatedCertificate) {
    std::string pem = readTestData("test-cert-a.pem");
    ASSERT_GT(pem.size(), std::size_t{200});
    pem.resize(pem.size() / 2);

    EXPECT_FALSE(identifyCertificate(pem).has_value());
}
