#include "core/auth/FileSessionRepository.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <utility>

#include "core/util/Logger.h"
#include "core/util/SecretStore.h"

namespace baludesk::core {
namespace {

// Getrennte Schlüssel, weil Windows ein einzelnes Geheimnis bei
// CRED_MAX_CREDENTIAL_BLOB_SIZE (2560 Bytes) deckelt und zwei JWT zusammen
// daran kratzen können.
constexpr const char* kAccessTokenKey = "session/access-token";
constexpr const char* kRefreshTokenKey = "session/refresh-token";

}  // namespace

FileSessionRepository::FileSessionRepository(std::string jsonPath, SecretStore& secrets)
    : jsonPath_(std::move(jsonPath)), secrets_(secrets) {}

std::optional<Session> FileSessionRepository::load() {
    std::ifstream stream(jsonPath_, std::ios::binary);
    if (!stream.good()) {
        return std::nullopt;
    }

    const std::string contents((std::istreambuf_iterator<char>(stream)),
                               std::istreambuf_iterator<char>());
    auto document = nlohmann::json::parse(contents, nullptr, false);
    if (document.is_discarded() || !document.is_object()) {
        Logger::warn("Sitzung: Ablagedatei ist unlesbar, wird ignoriert");
        return std::nullopt;
    }

    Session session;
    const auto readString = [&document](const char* name) -> std::string {
        const auto it = document.find(name);
        if (it == document.end() || !it->is_string()) {
            return {};
        }
        return it->get<std::string>();
    };

    session.serverUrl = readString("server_url");
    session.username = readString("username");
    session.publicKeyPin = readString("public_key_pin");

    const auto accessToken = secrets_.load(kAccessTokenKey);
    const auto refreshToken = secrets_.load(kRefreshTokenKey);
    if (accessToken) {
        session.accessToken = *accessToken;
    }
    if (refreshToken) {
        session.refreshToken = *refreshToken;
    }

    if (!session.isValid()) {
        // Datei ohne Geheimnisse oder umgekehrt. Eine halbe Sitzung ist
        // keine - lieber neu paaren als mit einem Torso weiterarbeiten.
        Logger::info("Sitzung: unvollständig abgelegt, wird verworfen");
        return std::nullopt;
    }

    return session;
}

bool FileSessionRepository::save(const Session& session) {
    // Die Geheimnisse zuerst: der Server liefert die Token genau einmal aus.
    // Scheitert das Ablegen, darf nichts zurückbleiben, was wie eine gültige
    // Sitzung aussieht.
    if (!secrets_.save(kAccessTokenKey, session.accessToken)) {
        Logger::error("Sitzung: Zugangstoken konnte nicht abgelegt werden");
        return false;
    }
    if (!secrets_.save(kRefreshTokenKey, session.refreshToken)) {
        Logger::error("Sitzung: Erneuerungstoken konnte nicht abgelegt werden");
        secrets_.erase(kAccessTokenKey);
        return false;
    }

    std::error_code error;
    const std::filesystem::path path(jsonPath_);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), error);
    }

    nlohmann::json document;
    document["server_url"] = session.serverUrl;
    document["username"] = session.username;
    document["public_key_pin"] = session.publicKeyPin;

    std::ofstream stream(jsonPath_, std::ios::binary | std::ios::trunc);
    if (!stream.good()) {
        Logger::error("Sitzung: Ablagedatei konnte nicht geschrieben werden");
        secrets_.erase(kAccessTokenKey);
        secrets_.erase(kRefreshTokenKey);
        return false;
    }
    stream << document.dump(2);
    stream.close();

    if (!stream.good()) {
        Logger::error("Sitzung: Ablagedatei konnte nicht vollständig geschrieben werden");
        secrets_.erase(kAccessTokenKey);
        secrets_.erase(kRefreshTokenKey);
        return false;
    }

    Logger::info("Sitzung: abgelegt für Benutzer {}", session.username);
    return true;
}

void FileSessionRepository::clear() {
    secrets_.erase(kAccessTokenKey);
    secrets_.erase(kRefreshTokenKey);

    std::error_code error;
    std::filesystem::remove(jsonPath_, error);

    Logger::info("Sitzung: entfernt");
}

}  // namespace baludesk::core
