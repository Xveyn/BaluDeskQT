#include "core/auth/FileSessionRepository.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "support/FakeSecretStore.h"

namespace fs = std::filesystem;
using baludesk::core::FileSessionRepository;
using baludesk::core::Session;
using baludesk::core::test::FakeSecretStore;

namespace {

fs::path makeEmptyTempDir(const std::string& name) {
    const fs::path directory = fs::temp_directory_path() / ("baludesk_session_test_" + name);
    fs::remove_all(directory);
    fs::create_directories(directory);
    return directory;
}

Session exampleSession() {
    Session session;
    session.serverUrl = "https://nas.example.com";
    session.username = "sven";
    session.publicKeyPin = "sha256//AAAABBBBCCCC=";
    session.accessToken = "zugangstoken";
    session.refreshToken = "erneuerungstoken";
    return session;
}

}  // namespace

TEST(SessionRepositoryTest, ReportsNothingWhenNoSessionWasStored) {
    const auto directory = makeEmptyTempDir("leer");
    FakeSecretStore secrets;
    FileSessionRepository repository((directory / "session.json").string(), secrets);

    EXPECT_FALSE(repository.load().has_value());
}

TEST(SessionRepositoryTest, RoundTripsASession) {
    const auto directory = makeEmptyTempDir("rundlauf");
    FakeSecretStore secrets;
    const std::string path = (directory / "session.json").string();

    {
        FileSessionRepository repository(path, secrets);
        ASSERT_TRUE(repository.save(exampleSession()));
    }

    FileSessionRepository reopened(path, secrets);
    const auto loaded = reopened.load();
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->serverUrl, "https://nas.example.com");
    EXPECT_EQ(loaded->username, "sven");
    EXPECT_EQ(loaded->publicKeyPin, "sha256//AAAABBBBCCCC=");
    EXPECT_EQ(loaded->accessToken, "zugangstoken");
    EXPECT_EQ(loaded->refreshToken, "erneuerungstoken");
}

TEST(SessionRepositoryTest, KeepsTokensOutOfTheFile) {
    const auto directory = makeEmptyTempDir("dateiinhalt");
    FakeSecretStore secrets;
    const std::string path = (directory / "session.json").string();

    FileSessionRepository repository(path, secrets);
    ASSERT_TRUE(repository.save(exampleSession()));

    std::ifstream stream(path, std::ios::binary);
    const std::string contents((std::istreambuf_iterator<char>(stream)),
                               std::istreambuf_iterator<char>());

    // Der entscheidende Test dieser Klasse: die Datei darf die Adresse, den
    // Benutzernamen und den Pin enthalten - aber niemals ein Token.
    EXPECT_NE(contents.find("nas.example.com"), std::string::npos);
    EXPECT_NE(contents.find("sven"), std::string::npos);
    EXPECT_NE(contents.find("sha256//"), std::string::npos);
    EXPECT_EQ(contents.find("zugangstoken"), std::string::npos);
    EXPECT_EQ(contents.find("erneuerungstoken"), std::string::npos);
}

TEST(SessionRepositoryTest, StoresTheTwoTokensSeparately) {
    const auto directory = makeEmptyTempDir("getrennt");
    FakeSecretStore secrets;
    FileSessionRepository repository((directory / "session.json").string(), secrets);

    ASSERT_TRUE(repository.save(exampleSession()));

    // Getrennt abgelegt, weil Windows ein einzelnes Geheimnis bei 2560 Bytes
    // deckelt und zwei JWT zusammen daran kratzen können.
    ASSERT_EQ(secrets.entries.size(), std::size_t{2});
    bool foundAccess = false;
    bool foundRefresh = false;
    for (const auto& entry : secrets.entries) {
        if (entry.second == "zugangstoken") { foundAccess = true; }
        if (entry.second == "erneuerungstoken") { foundRefresh = true; }
    }
    EXPECT_TRUE(foundAccess);
    EXPECT_TRUE(foundRefresh);
}

TEST(SessionRepositoryTest, ReportsNothingWhenTheTokensAreGone) {
    const auto directory = makeEmptyTempDir("tokenweg");
    FakeSecretStore secrets;
    const std::string path = (directory / "session.json").string();

    FileSessionRepository repository(path, secrets);
    ASSERT_TRUE(repository.save(exampleSession()));

    // Datei ist da, Geheimnisse nicht - etwa weil ein anderes Benutzerprofil
    // zugreift. Eine halbe Sitzung ist keine Sitzung.
    secrets.entries.clear();

    EXPECT_FALSE(repository.load().has_value());
}

TEST(SessionRepositoryTest, ReportsNothingWhenTheFileIsGarbage) {
    const auto directory = makeEmptyTempDir("kaputt");
    FakeSecretStore secrets;
    const std::string path = (directory / "session.json").string();

    {
        std::ofstream stream(path, std::ios::binary);
        stream << "das ist kein json";
    }

    FileSessionRepository repository(path, secrets);
    EXPECT_FALSE(repository.load().has_value());
}

TEST(SessionRepositoryTest, ReportsNothingWhenTheServerUrlIsMissing) {
    const auto directory = makeEmptyTempDir("unvollstaendig");
    FakeSecretStore secrets;
    const std::string path = (directory / "session.json").string();

    {
        std::ofstream stream(path, std::ios::binary);
        stream << R"({"username":"sven"})";
    }

    FileSessionRepository repository(path, secrets);
    EXPECT_FALSE(repository.load().has_value());
}

TEST(SessionRepositoryTest, ClearRemovesFileAndSecrets) {
    const auto directory = makeEmptyTempDir("abmelden");
    FakeSecretStore secrets;
    const std::string path = (directory / "session.json").string();

    FileSessionRepository repository(path, secrets);
    ASSERT_TRUE(repository.save(exampleSession()));
    ASSERT_TRUE(fs::exists(path));

    repository.clear();

    EXPECT_FALSE(fs::exists(path));
    EXPECT_TRUE(secrets.entries.empty());
    EXPECT_FALSE(repository.load().has_value());
}

TEST(SessionRepositoryTest, ClearOnAnEmptyRepositoryIsHarmless) {
    const auto directory = makeEmptyTempDir("leerraeumen");
    FakeSecretStore secrets;
    const std::string path = (directory / "session.json").string();
    FileSessionRepository repository(path, secrets);

    // Abmelden ohne angemeldet zu sein darf weder scheitern noch etwas
    // anlegen. Die Zusicherungen danach prüfen genau das - ein blosses
    // "es ist nicht abgestürzt" wäre zu wenig.
    repository.clear();

    EXPECT_FALSE(fs::exists(path));
    EXPECT_TRUE(secrets.entries.empty());
    EXPECT_FALSE(repository.load().has_value());
}

TEST(SessionRepositoryTest, ReportsFailureWhenTheSecretCannotBeStored) {
    const auto directory = makeEmptyTempDir("speicherfehler");
    FakeSecretStore secrets;
    secrets.failNextSave = true;
    FileSessionRepository repository((directory / "session.json").string(), secrets);

    // Ein Token, das nicht abgelegt werden konnte, darf nicht als
    // erfolgreiche Sitzung durchgehen - der Server liefert es kein zweites Mal.
    EXPECT_FALSE(repository.save(exampleSession()));
    EXPECT_FALSE(repository.load().has_value());
}

TEST(SessionRepositoryTest, OverwritesAPreviousSession) {
    const auto directory = makeEmptyTempDir("ueberschreiben");
    FakeSecretStore secrets;
    FileSessionRepository repository((directory / "session.json").string(), secrets);

    ASSERT_TRUE(repository.save(exampleSession()));

    Session second = exampleSession();
    second.username = "andrea";
    second.accessToken = "neues-zugangstoken";
    ASSERT_TRUE(repository.save(second));

    const auto loaded = repository.load();
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->username, "andrea");
    EXPECT_EQ(loaded->accessToken, "neues-zugangstoken");
}

TEST(SessionRepositoryTest, CreatesMissingDirectories) {
    const auto directory = makeEmptyTempDir("tiefer");
    FakeSecretStore secrets;
    const std::string path = (directory / "a" / "b" / "session.json").string();

    FileSessionRepository repository(path, secrets);
    EXPECT_TRUE(repository.save(exampleSession()));
    EXPECT_TRUE(fs::exists(path));
}

TEST(SessionTest, IsValidOnlyWithEverythingThatMatters) {
    Session session = exampleSession();
    EXPECT_TRUE(session.isValid());

    session = exampleSession();
    session.serverUrl.clear();
    EXPECT_FALSE(session.isValid());

    session = exampleSession();
    session.accessToken.clear();
    EXPECT_FALSE(session.isValid());

    session = exampleSession();
    session.refreshToken.clear();
    EXPECT_FALSE(session.isValid());

    // Ein Pin ist keine Pflicht - bei einem gültigen Zertifikat gibt es keinen.
    session = exampleSession();
    session.publicKeyPin.clear();
    EXPECT_TRUE(session.isValid());
}
