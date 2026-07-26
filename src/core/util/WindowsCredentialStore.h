#pragma once

#include "core/util/SecretStore.h"

#ifdef _WIN32

#include <optional>
#include <string>

namespace baludesk::core {

namespace detail {

// UTF-8 nach UTF-16. Steht sichtbar hier, damit die Wandlung gegen eine
// bekannte Bytefolge geprüft werden kann statt nur im Rundlauf durch sich
// selbst - ein Rundlauf wäre auch mit einer falschen Wandlung grün, solange
// Schreib- und Lesepfad dieselbe benutzen.
//
// Leer, wenn die Eingabe kein gültiges UTF-8 ist.
[[nodiscard]] std::wstring utf8ToWide(const std::string& utf8);

}  // namespace detail

// Die Windows-Realisierung von SecretStore: Credential Manager (wincred).
//
// Der Schlüssel ist ein freier Bezeichner wie "server/access-token". Er wird
// mit einem festen Präfix versehen, damit BaluDesks Einträge im Credential
// Manager erkennbar sind und nicht mit fremden kollidieren.
class WindowsCredentialStore : public SecretStore {
public:
    bool save(const std::string& key, const std::string& secret) override;
    [[nodiscard]] std::optional<std::string> load(const std::string& key) override;
    bool erase(const std::string& key) override;
    [[nodiscard]] bool contains(const std::string& key) override;
};

}  // namespace baludesk::core

#endif  // _WIN32
