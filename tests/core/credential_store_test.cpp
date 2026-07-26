#include "core/util/CredentialStore.h"

#include <gtest/gtest.h>

#include <string>

#include "core/util/SecretStore.h"
#include "core/util/WindowsCredentialStore.h"

#ifdef _WIN32
#include <windows.h>

#include <wincred.h>
#endif

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
    // Prüft den echten Rundlauf über den Windows Credential Manager mit
    // Umlauten in Schlüssel und Wert. Die eigentliche Absicherung gegen die
    // byteweise Alt-Client-Verbreiterung liefern die
    // CredentialStoreConversionTest-Fälle weiter unten: Ein Rundlauf allein
    // bliebe grün, selbst wenn Schreib- und Lesepfad dieselbe falsche
    // Wandlung benutzten - Windows vergleicht Zielnamen nur als
    // undurchsichtige Kennung.
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

#ifdef _WIN32

TEST_F(CredentialStoreTest, RejectsASecretOneByteOverTheBlobLimit) {
    // Windows begrenzt CredentialBlob auf CRED_MAX_CREDENTIAL_BLOB_SIZE Bytes.
    // Ein Byte darüber muss abgelehnt werden, statt CredWriteW mit einem
    // undurchsichtigen Fehlercode scheitern zu lassen.
    const std::string secret(static_cast<std::size_t>(CRED_MAX_CREDENTIAL_BLOB_SIZE) + 1, 'x');
    EXPECT_FALSE(CredentialStore::save(kKeyAscii, secret));
    EXPECT_FALSE(CredentialStore::contains(kKeyAscii));
}

TEST_F(CredentialStoreTest, AcceptsASecretExactlyAtTheBlobLimit) {
    const std::string secret(static_cast<std::size_t>(CRED_MAX_CREDENTIAL_BLOB_SIZE), 'x');
    ASSERT_TRUE(CredentialStore::save(kKeyAscii, secret));

    const auto loaded = CredentialStore::load(kKeyAscii);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->size(), secret.size());
}

TEST_F(CredentialStoreTest, SecretStoreInterfaceWorksPolymorphically) {
    // Belegt, dass WindowsCredentialStore sich hinter der portablen
    // SecretStore-Schnittstelle genauso verhält wie hinter der statischen
    // CredentialStore-Fassade - der eigentliche Nutzen der Naht für Stufe 1b.
    baludesk::core::WindowsCredentialStore windowsStore;
    baludesk::core::SecretStore& store = windowsStore;

    ASSERT_TRUE(store.save(kKeyAscii, "polymorph-wert"));
    EXPECT_TRUE(store.contains(kKeyAscii));

    const auto loaded = store.load(kKeyAscii);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(*loaded, "polymorph-wert");

    EXPECT_TRUE(store.erase(kKeyAscii));
    EXPECT_FALSE(store.contains(kKeyAscii));
}

TEST(CredentialStoreConversionTest, DecodesUtf8InsteadOfWideningBytes) {
    // "grüße" — ü und ß liegen in UTF-8 als Zweibytefolgen vor, die
    // Zeichenkette ist also 7 Bytes lang bei 5 Zeichen.
    const std::string utf8 = "gr\xC3\xBC\xC3\x9F" "e";
    const std::wstring wide = baludesk::core::detail::utf8ToWide(utf8);

    // Genau hier scheiden sich die beiden Wandlungen: eine echte Dekodierung
    // liefert 5 Zeichen, die byteweise Verbreiterung des Alt-Clients liefert
    // 7 - für jedes Byte eines. Die Längenprüfung allein erledigt den
    // Regressionsschutz, die Einzelwerte belegen zusätzlich die Kodierung.
    ASSERT_EQ(wide.size(), std::size_t{5});
    EXPECT_EQ(wide[0], L'g');
    EXPECT_EQ(wide[1], L'r');
    EXPECT_EQ(wide[2], static_cast<wchar_t>(0x00FC));  // ü
    EXPECT_EQ(wide[3], static_cast<wchar_t>(0x00DF));  // ß
    EXPECT_EQ(wide[4], L'e');
}

TEST(CredentialStoreConversionTest, RejectsInvalidUtf8) {
    // 0xFF kommt in gültigem UTF-8 nie vor.
    EXPECT_TRUE(baludesk::core::detail::utf8ToWide(std::string("\xFF", 1)).empty());
}

TEST(CredentialStoreConversionTest, MapsEmptyInputToEmptyOutput) {
    EXPECT_TRUE(baludesk::core::detail::utf8ToWide("").empty());
}

#endif
