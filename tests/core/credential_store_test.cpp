#include "core/util/CredentialStore.h"

#include <gtest/gtest.h>

#include <string>

using baludesk::core::CredentialStore;

namespace {

// Eigenes Präfix, damit die Tests keine echten Anmeldedaten anfassen.
const std::string kKeyAscii = "baludesk-test/plain";
const std::string kKeyUmlauts = "baludesk-test/grüße-für-müller";

}  // namespace

class CredentialStoreTest : public ::testing::Test {
protected:
    void SetUp() override { cleanUp(); }
    void TearDown() override { cleanUp(); }

private:
    static void cleanUp() {
        CredentialStore::erase(kKeyAscii);
        CredentialStore::erase(kKeyUmlauts);
    }
};

TEST_F(CredentialStoreTest, ReportsNothingForAnUnknownKey) {
    EXPECT_FALSE(CredentialStore::contains(kKeyAscii));
    EXPECT_FALSE(CredentialStore::load(kKeyAscii).has_value());
}

TEST_F(CredentialStoreTest, RoundTripsASecret) {
    ASSERT_TRUE(CredentialStore::save(kKeyAscii, "geheimer-wert-123"));

    EXPECT_TRUE(CredentialStore::contains(kKeyAscii));
    const auto loaded = CredentialStore::load(kKeyAscii);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(*loaded, "geheimer-wert-123");
}

TEST_F(CredentialStoreTest, SurvivesUmlautsInKeyAndValue) {
    // Der Alt-Client verbreiterte byteweise nach wchar_t und verstümmelte
    // damit jedes Zeichen jenseits von ASCII. Dieser Test hält das fest.
    const std::string secret = "Straße-Öl-Übermut-ß";
    ASSERT_TRUE(CredentialStore::save(kKeyUmlauts, secret));

    const auto loaded = CredentialStore::load(kKeyUmlauts);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(*loaded, secret);
}

TEST_F(CredentialStoreTest, OverwritesAnExistingSecret) {
    ASSERT_TRUE(CredentialStore::save(kKeyAscii, "erster-wert"));
    ASSERT_TRUE(CredentialStore::save(kKeyAscii, "zweiter-wert"));

    const auto loaded = CredentialStore::load(kKeyAscii);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(*loaded, "zweiter-wert");
}

TEST_F(CredentialStoreTest, ErasesASecret) {
    ASSERT_TRUE(CredentialStore::save(kKeyAscii, "wird-gelöscht"));
    ASSERT_TRUE(CredentialStore::erase(kKeyAscii));

    EXPECT_FALSE(CredentialStore::contains(kKeyAscii));
    EXPECT_FALSE(CredentialStore::load(kKeyAscii).has_value());
}

TEST_F(CredentialStoreTest, ErasingSomethingAbsentIsNotAnError) {
    EXPECT_FALSE(CredentialStore::contains(kKeyAscii));
    EXPECT_TRUE(CredentialStore::erase(kKeyAscii));
}

TEST_F(CredentialStoreTest, StoresBinarySafeValues) {
    // Tokens sind Text, aber der Speicher darf an einem Nullbyte nicht
    // abschneiden - sonst fällt das erst bei einem Sonderfall auf.
    const std::string secret = std::string("vorne\0hinten", 12);
    ASSERT_EQ(secret.size(), std::size_t{12});
    ASSERT_TRUE(CredentialStore::save(kKeyAscii, secret));

    const auto loaded = CredentialStore::load(kKeyAscii);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(*loaded, secret);
    EXPECT_EQ(loaded->size(), std::size_t{12});
}

TEST_F(CredentialStoreTest, RejectsAnEmptyKey) {
    EXPECT_FALSE(CredentialStore::save("", "egal"));
    EXPECT_FALSE(CredentialStore::load("").has_value());
    EXPECT_FALSE(CredentialStore::contains(""));
}
