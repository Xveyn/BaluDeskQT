#include "core/auth/FileSessionRepository.h"

#include <nlohmann/json.hpp>

#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <utility>

#include "core/api/ServerUrl.h"
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

    // Auch eine wiedereingelesene Adresse muss durch dieselbe Prüfung wie
    // jeder andere Aufrufer von checkServerUrl - genau dafür bindet
    // ServerUrl.h die Funktion für JEDEN Aufrufer, nicht nur den
    // Eingabedialog. Ohne das könnte eine von Hand bearbeitete oder aus
    // einer anderen Umgebung kopierte session.json ein "http://" zu einem
    // fremden Rechner einschleusen - der Bearer-Token ginge dann blank über
    // diese Verbindung.
    const auto urlCheck = checkServerUrl(session.serverUrl);
    if (!urlCheck.ok()) {
        Logger::warn("Sitzung: abgelegte Serveradresse ist ungültig ({}), wird verworfen",
                     urlCheck.message);
        return std::nullopt;
    }
    session.serverUrl = urlCheck.normalized;

    return session;
}

bool FileSessionRepository::save(const Session& session) {
    // Bisherigen Stand sichern. Ohne das würde ein misslungenes Aktualisieren
    // die bestehende Sitzung zerstören: der Rückbau löschte den Schlüssel und
    // damit auch den alten Wert — und der Server liefert Token kein zweites
    // Mal aus.
    const std::optional<std::string> previousAccessToken = secrets_.load(kAccessTokenKey);
    const std::optional<std::string> previousRefreshToken = secrets_.load(kRefreshTokenKey);

    const auto rollbackSecrets = [&]() {
        const auto restore = [this](const char* key, const std::optional<std::string>& previous) {
            const bool restored = previous ? secrets_.save(key, *previous) : secrets_.erase(key);
            if (!restored) {
                // Mehr ist an dieser Stelle nicht zu tun — aber wer die Datei
                // später untersucht, soll wissen, dass etwas zurückblieb.
                Logger::error("Sitzung: Rückbau von „{}“ fehlgeschlagen", key);
            }
        };
        restore(kAccessTokenKey, previousAccessToken);
        restore(kRefreshTokenKey, previousRefreshToken);
    };

    // Die Geheimnisse zuerst: der Server liefert die Token genau einmal aus.
    // Scheitert das Ablegen, darf nichts zurückbleiben, was wie eine gültige
    // Sitzung aussieht - und eine etwa schon bestehende Sitzung muss unversehrt
    // bleiben statt gelöscht zu werden.
    if (!secrets_.save(kAccessTokenKey, session.accessToken)) {
        Logger::error("Sitzung: Zugangstoken konnte nicht abgelegt werden");
        rollbackSecrets();
        return false;
    }
    if (!secrets_.save(kRefreshTokenKey, session.refreshToken)) {
        Logger::error("Sitzung: Erneuerungstoken konnte nicht abgelegt werden");
        rollbackSecrets();
        return false;
    }

    std::error_code error;
    const std::filesystem::path path(jsonPath_);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), error);
    }

    // Erst als Text im Speicher aufbauen und in eine Nachbardatei schreiben,
    // dann per rename() über das Ziel legen. Ein std::ofstream mit
    // std::ios::trunc direkt auf jsonPath_ würde die bestehende Datei schon
    // beim Öffnen zerstören - schlägt der Schreibvorgang danach fehl, wäre
    // eine intakte Sitzung trotz Rückbau der Geheimnisse futsch, weil load()
    // die abgeschnittene Datei verwirft. Bis der rename() glückt, bleibt
    // jsonPath_ unangetastet.
    const std::filesystem::path tempPath = jsonPath_ + ".tmp";

    std::string serialized;
    try {
        nlohmann::json document;
        document["server_url"] = session.serverUrl;
        document["username"] = session.username;
        document["public_key_pin"] = session.publicKeyPin;
        // error_handler_t::replace statt des Standards (strict): ungültiges
        // UTF-8 wird ersetzt statt eine Ausnahme zu werfen. An dieser Stelle
        // sind die Geheimnisse bereits abgelegt - eine hier durchschlagende
        // Ausnahme wäre genau der halb abgelegte Zustand, den
        // SessionRepository.h ausschließt, und ein Absturz statt eines
        // false-Rückgabewerts widerspräche dem dokumentierten Vertrag.
        serialized = document.dump(2, ' ', false, nlohmann::json::error_handler_t::replace);
    } catch (const std::exception& ex) {
        Logger::error("Sitzung: Ablagedatei konnte nicht aufgebaut werden — {}", ex.what());
        rollbackSecrets();
        return false;
    }

    try {
        std::ofstream stream(tempPath, std::ios::binary | std::ios::trunc);
        if (!stream.good()) {
            Logger::error("Sitzung: temporäre Ablagedatei konnte nicht geschrieben werden");
            std::error_code removeError;
            std::filesystem::remove(tempPath, removeError);
            rollbackSecrets();
            return false;
        }
        stream << serialized;
        stream.close();

        if (!stream.good()) {
            Logger::error("Sitzung: temporäre Ablagedatei konnte nicht vollständig geschrieben "
                          "werden");
            std::error_code removeError;
            std::filesystem::remove(tempPath, removeError);
            rollbackSecrets();
            return false;
        }
    } catch (const std::exception& ex) {
        Logger::error("Sitzung: Schreiben der temporären Ablagedatei ist fehlgeschlagen — {}",
                      ex.what());
        std::error_code removeError;
        std::filesystem::remove(tempPath, removeError);
        rollbackSecrets();
        return false;
    }

    std::error_code renameError;
    std::filesystem::rename(tempPath, path, renameError);
    if (renameError) {
        Logger::error("Sitzung: Ablagedatei konnte nicht ersetzt werden — {}",
                      renameError.message());
        std::error_code removeError;
        std::filesystem::remove(tempPath, removeError);
        rollbackSecrets();
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
