#include "core/util/CredentialStore.h"

#ifdef _WIN32

namespace baludesk::core {
namespace {

// Eine Instanz je Prozess reicht: WindowsCredentialStore hält keinen eigenen
// Zustand, jeder Aufruf geht direkt an den Credential Manager des
// Betriebssystems.
WindowsCredentialStore& instance() {
    static WindowsCredentialStore store;
    return store;
}

}  // namespace

bool CredentialStore::save(const std::string& key, const std::string& secret) {
    return instance().save(key, secret);
}

std::optional<std::string> CredentialStore::load(const std::string& key) {
    return instance().load(key);
}

bool CredentialStore::erase(const std::string& key) {
    return instance().erase(key);
}

bool CredentialStore::contains(const std::string& key) {
    return instance().contains(key);
}

}  // namespace baludesk::core

#endif  // _WIN32
