#include "core/util/CredentialStore.h"

#include "core/util/Logger.h"

#ifdef _WIN32

#include <windows.h>
#include <wincred.h>

#include <memory>
#include <string>

namespace baludesk::core {
namespace {

// Präfix für alle Einträge, damit BaluDesks Anmeldeinformationen im Credential
// Manager erkennbar sind und nicht mit fremden kollidieren. Bewusst hier und
// nicht im Header: außerhalb dieser Datei braucht es niemand.
constexpr const char* kTargetPrefix = "BaluDesk:";

// UTF-8 nach UTF-16. Der Alt-Client schrieb hier std::wstring(s.begin(),
// s.end()) und verbreiterte damit jedes Byte einzeln - alles jenseits von
// ASCII wurde dabei zerlegt.
std::wstring toWide(const std::string& utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int needed = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (needed <= 0) {
        return {};
    }
    std::wstring wide(static_cast<std::size_t>(needed), L'\0');
    const int written = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(utf8.size()),
        wide.data(), needed);
    if (written != needed) {
        return {};
    }
    return wide;
}

std::wstring targetNameFor(const std::string& key) {
    return toWide(std::string(kTargetPrefix) + key);
}

struct CredentialDeleter {
    void operator()(CREDENTIALW* credential) const noexcept { CredFree(credential); }
};
using CredentialPtr = std::unique_ptr<CREDENTIALW, CredentialDeleter>;

CredentialPtr readCredential(const std::string& key) {
    const std::wstring target = targetNameFor(key);
    if (target.empty()) {
        return nullptr;
    }

    CREDENTIALW* raw = nullptr;
    if (CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &raw) != TRUE) {
        const DWORD error = GetLastError();
        if (error != ERROR_NOT_FOUND) {
            Logger::warn("CredentialStore: Lesen fehlgeschlagen, Fehlercode {}", error);
        }
        return nullptr;
    }
    return CredentialPtr(raw);
}

}  // namespace

bool CredentialStore::save(const std::string& key, const std::string& secret) {
    if (key.empty()) {
        return false;
    }

    std::wstring target = targetNameFor(key);
    if (target.empty()) {
        Logger::error("CredentialStore: Schlüssel nicht nach UTF-16 wandelbar");
        return false;
    }

    CREDENTIALW credential = {};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = target.data();
    credential.CredentialBlobSize = static_cast<DWORD>(secret.size());
    credential.CredentialBlob =
        reinterpret_cast<LPBYTE>(const_cast<char*>(secret.data()));
    // CRED_PERSIST_LOCAL_MACHINE: bleibt über Neuanmeldungen erhalten, wandert
    // aber nicht mit einem servergespeicherten Profil auf andere Rechner. Ein
    // Zugangstoken gehört an genau ein Gerät.
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;

    if (CredWriteW(&credential, 0) != TRUE) {
        Logger::error("CredentialStore: Schreiben fehlgeschlagen, Fehlercode {}", GetLastError());
        return false;
    }
    return true;
}

std::optional<std::string> CredentialStore::load(const std::string& key) {
    if (key.empty()) {
        return std::nullopt;
    }

    const CredentialPtr credential = readCredential(key);
    if (!credential) {
        return std::nullopt;
    }

    return std::string(reinterpret_cast<const char*>(credential->CredentialBlob),
                       static_cast<std::size_t>(credential->CredentialBlobSize));
}

bool CredentialStore::erase(const std::string& key) {
    if (key.empty()) {
        return false;
    }

    const std::wstring target = targetNameFor(key);
    if (target.empty()) {
        return false;
    }

    if (CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0) != TRUE) {
        const DWORD error = GetLastError();
        if (error == ERROR_NOT_FOUND) {
            // Nichts zu löschen ist das gewünschte Ergebnis.
            return true;
        }
        Logger::error("CredentialStore: Löschen fehlgeschlagen, Fehlercode {}", error);
        return false;
    }
    return true;
}

bool CredentialStore::contains(const std::string& key) {
    if (key.empty()) {
        return false;
    }
    return readCredential(key) != nullptr;
}

}  // namespace baludesk::core

#endif  // _WIN32
