# BaluDesk QT — Stufe 1b (Pairing-Ablauf und Sitzung) Implementierungsplan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** BaluDesk kann sich per Gerätecode mit einem BaluHost-Server paaren, die Sitzung dauerhaft ablegen und sie nach einem Neustart ohne erneutes Paaren wiederherstellen — belegt durch eine echte Paarung gegen den laufenden Server, gefahren über die Kommandozeile.

**Architecture:** Alles Neue liegt in `baludesk_core` und bleibt Qt-frei, damit das Prüfprogramm denselben Code fährt wie später die Oberfläche. Der `PairingClient` ist ein Zustandsautomat ohne eigenes Zeitgefühl: er bekommt die aktuelle Zeit hereingereicht und gibt zurück, wann er das nächste Mal gefragt werden will. Dadurch ist das gesamte Zeitverhalten — Intervall, Ablauf, Ratenbegrenzung — ohne einen einzigen echten Wartevorgang prüfbar.

**Tech Stack:** C++17 · libcurl 8.21 mit OpenSSL · nlohmann/json · spdlog · GoogleTest · Qt 6.8.0 (in dieser Stufe nicht berührt)

**Spec:** `docs/superpowers/specs/2026-07-26-baludesk-qt-design.md` — Abschnitt 2.6, 2.9, 2.10, 12 Stufe 1
**Übergabe aus 1a:** `docs/superpowers/uebergabe-an-stufe-1b.md`
**Vorgänger:** `docs/superpowers/plans/2026-07-26-stufe-1a-transport-und-zertifikatsvertrauen.md`, abgenommen, 60 Tests grün

**Nachfolger:** Stufe 1c bringt die drei Dialoge (Server-URL, Zertifikat bestätigen, Code anzeigen), die Einbindung ins Hauptfenster und das Abmelden über die Oberfläche. Dieser Plan endet, sobald eine Paarung von der Kommandozeile aus nachweislich funktioniert und einen Neustart überlebt.

## Global Constraints

- **C++17**, `CMAKE_CXX_EXTENSIONS OFF`, `cmake_minimum_required(VERSION 3.25)`
- **`baludesk_core` darf niemals Qt linken.** Alles in diesem Plan liegt darin — kein `QString`, kein `QObject`, kein Qt-Header. Die Absicherung steht in der Wurzel-`CMakeLists.txt` nach `add_subdirectory(tests)`.
- **Namensräume:** `baludesk::core` für alles unter `src/core/`
- **Include-Pfade immer vollständig ab `src/`**, also `#include "core/auth/PairingClient.h"`, nie relativ mit `../`
- **Sprache:** Bezeichner und Dateinamen englisch, Kommentare und Protokolltexte deutsch
- **Umlaute:** In allen Dateien echte UTF-8-Zeichen (`ü`, `ö`, `ä`, `ß`), gespeichert als UTF-8 **ohne BOM**. Keine Behelfsschreibung. **Einzige Ausnahme: Git-Commit-Nachrichten bleiben ASCII**, weil PowerShell-5.1-Here-Strings Umlaute verstümmeln.
- **Generator:** `Visual Studio 17 2022`, x64. Warnungen `/W4 /permissive- /utf-8`, keine Warnungen aus eigenem Code.
- **Shell ist PowerShell 5.1:** kein `&&`, mit `;` verketten, Pfade quoten.
- **Nicht in Stufe 1b:** Qt-Dialoge, Hauptfenster-Einbindung, Abmelden über die Oberfläche, Sync, Datenbank, Infobereichssymbol, Dashboard

---

## Server-Vertrag

Gegen BaluHost v1.38.0 am 2026-07-26 verifiziert, teils durch Lesen des Dienstquelltexts.

### `POST /api/desktop-pairing/device-code`

Ohne Anmeldung. Ratenlimit **3 je Minute**.

```json
{ "device_id": "…", "device_name": "…", "platform": "windows" }
```

`platform` muss auf `^(windows|mac|linux)$` passen. Antwort:

```json
{ "device_code": "…", "user_code": "483920",
  "verification_url": "https://server/devices?pair=1",
  "expires_in": 600, "interval": 5 }
```

### `POST /api/desktop-pairing/poll`

Ohne Anmeldung. Ratenlimit **12 je Minute** bei einem vorgegebenen `interval` von 5 Sekunden — **exakt ein Abruf je Intervall, ohne jede Reserve.** Wer schneller fragt, sperrt sich aus.

```json
{ "device_code": "…" }
```

Antwort `{ "status": … }` mit `status` aus `authorization_pending`, `approved`, `expired`, `denied`. Bei `approved` zusätzlich:

```json
{ "status": "approved", "access_token": "…", "refresh_token": "…",
  "token_type": "bearer",
  "user": { "id": 1, "username": "sven", "email": "…", "role": "admin" } }
```

**Die Zustellung ist einmalig.** Der Server löscht den Pairing-Datensatz, sobald er `approved` samt Token ausgeliefert hat. Ein zweiter Abruf mit demselben `device_code` liefert **HTTP 404 „Invalid device code"** — nicht noch einmal die Token. Daraus folgt die wichtigste Regel dieses Plans: **die Token werden abgelegt, bevor irgendetwas anderes passiert.** Wer zwischen Empfang und Speichern abstürzt, muss neu paaren.

Ein unbekannter `device_code` liefert ebenfalls 404. Die Geräteregistrierung legt der Server bei der Freigabe selbst an — ein eigener Aufruf von `/api/sync/register-desktop` entfällt.

### Ratenbegrenzung

Bei Überschreitung antwortet der Server mit **HTTP 429** und diesem Rumpf:

```json
{ "error": "Too Many Requests", "detail": "Rate limit exceeded. Please try again later.", "retry_after": 60 }
```

Derselbe Wert steht zusätzlich im `Retry-After`-Kopf. `HttpResponse` trägt keine Kopfzeilen — der Wert wird deshalb **aus dem Rumpf** gelesen. Das genügt und erspart einen Umbau der Transportschicht.

### `POST /api/auth/refresh`

`{ "refresh_token": "…" }` → `{ "access_token": "…", "token_type": "bearer", "user": {…} }`

**Das Erneuerungstoken wird nicht rotiert.** Läuft es ab, hilft kein Erneuern mehr — dann muss neu gepaart werden. Dieser Fall ist von einem gewöhnlichen 401 zu unterscheiden, sonst landet der Nutzer in einer Schleife.

---

## Vorhandene Bausteine aus Stufe 1a

Alle Qt-frei in `baludesk_core`:

| Baustein | Was er kann |
|---|---|
| `core/api/HttpClient` | `get(path, bearerToken)`, `postJson(path, body, bearerToken)` → `HttpResult{error, statusCode, body, detail, peerCertificatePem, ok()}` |
| `core/api/Transport` + `CurlTransport` | Naht und libcurl-Umsetzung, mehrfach erzeugbar |
| `core/api/TlsPolicy` | `Mode::SystemTrust` / `Mode::Pinned`, `publicKeyPin`. Leerer Pin fällt auf volle Prüfung zurück |
| `core/util/TlsTrust` | `identifyCertificate(pem)` → `optional<CertificateIdentity>{fingerprintSha256, publicKeyPin}` |
| `core/util/SecretStore` + `WindowsCredentialStore` | Geheimnisse im Anmeldeinformationsspeicher, gedeckelt bei `CRED_MAX_CREDENTIAL_BLOB_SIZE` |
| `core/util/Logger` | `debug`/`info`/`warn`/`error` mit spdlog-Formatzeichenketten |
| `tests/core/support/FakeTransport.h` | Warteschlange von Antworten — genau für Poll-Abläufe gebaut |
| `tools/probe` | Prüfprogramm hinter der Option `BALUDESK_BUILD_TOOLS` |

`HttpErrorKind` kennt `None`, `Network`, `CertificateUntrusted`, `CertificatePinMismatch`, `Timeout`, `Protocol`.

---

## Dateistruktur nach Abschluss

```
src/core/
├── CMakeLists.txt              # + neue Quelldateien
├── api/
│   ├── ServerUrl.h/.cpp        # Adresse prüfen und normalisieren
│   └── TlsPolicy.h             # + benannte Erzeugerfunktion
├── auth/
│   ├── PairingTypes.h          # PairingState, PairingStep, PairingOutcome
│   ├── PairingClient.h/.cpp    # der Zustandsautomat
│   ├── Session.h               # Werttyp
│   ├── SessionRepository.h     # Schnittstelle
│   └── FileSessionRepository.h/.cpp  # JSON-Datei + SecretStore
tests/core/
├── server_url_test.cpp
├── pairing_client_test.cpp
├── session_repository_test.cpp
└── support/FakeSecretStore.h   # für die Repository-Tests
tools/probe/
└── main.cpp                    # + Unterbefehle pair / status / logout
```

**Warum die Sitzung nicht in `QSettings` liegt:** Das Prüfprogramm muss dieselbe Ablage benutzen wie später die Oberfläche, sonst beweist die Abnahme nichts über den echten Betrieb — und es ist Qt-frei. Die Sitzung gehört ohnehin dem Kern, nicht der Oberfläche. `SettingsStore`/`AppConfig` bleiben davon unberührt: das sind Bedienvorlieben, die der Oberfläche gehören. Zwei Ablagen mit zwei klaren Eigentümern, nicht zwei Wege für dieselbe Sache.

---

## Task 1: Serveradresse prüfen und den Pin an eine Entscheidung binden

**Files:**
- Create: `src/core/api/ServerUrl.h`
- Create: `src/core/api/ServerUrl.cpp`
- Modify: `src/core/api/TlsPolicy.h`
- Modify: `src/core/CMakeLists.txt`
- Test: `tests/core/server_url_test.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: nichts aus früheren Tasks dieses Plans
- Produces:
  - `enum class baludesk::core::ServerUrlProblem { None, Empty, Malformed, UnsupportedScheme, InsecureScheme }`
  - `struct baludesk::core::ServerUrlCheck { ServerUrlProblem problem; std::string normalized; std::string message; bool ok() const; }`
  - `ServerUrlCheck baludesk::core::checkServerUrl(const std::string& input)`
  - `TlsPolicy baludesk::core::TlsPolicy::pinnedFromConfirmedCertificate(const std::string& publicKeyPin)`

---

- [ ] **Step 1: Den fehlschlagenden Test schreiben**

Die Spec verlangt in Abschnitt 2.9 „HTTP bleibt allein für `localhost` erlaubt". Bisher hat diese Regel keinen Ort im Code: `HttpClient` nimmt jede Basis-URL und `CurlTransport` schickt bereitwillig ein `Authorization: Bearer …` über blankes `http://`. Stufe 1c ist die Stelle, an der ein Mensch zum ersten Mal eine Adresse eintippt — die Prüfung gehört aber hierher, damit sie für **jeden** Aufrufer gilt und nicht nur für den Dialog.

Datei `tests/core/server_url_test.cpp`:

```cpp
#include "core/api/ServerUrl.h"

#include <gtest/gtest.h>

#include <string>

using baludesk::core::checkServerUrl;
using baludesk::core::ServerUrlProblem;

TEST(ServerUrlTest, AcceptsPlainHttps) {
    const auto check = checkServerUrl("https://nas.example.com");
    EXPECT_TRUE(check.ok());
    EXPECT_EQ(check.problem, ServerUrlProblem::None);
    EXPECT_EQ(check.normalized, "https://nas.example.com");
}

TEST(ServerUrlTest, AcceptsHttpsWithPortAndPath) {
    const auto check = checkServerUrl("https://nas.example.com:8443");
    EXPECT_TRUE(check.ok());
    EXPECT_EQ(check.normalized, "https://nas.example.com:8443");
}

TEST(ServerUrlTest, StripsTrailingSlashes) {
    EXPECT_EQ(checkServerUrl("https://nas.example.com/").normalized, "https://nas.example.com");
    EXPECT_EQ(checkServerUrl("https://nas.example.com///").normalized, "https://nas.example.com");
}

TEST(ServerUrlTest, TrimsSurroundingWhitespace) {
    const auto check = checkServerUrl("   https://nas.example.com  ");
    EXPECT_TRUE(check.ok());
    EXPECT_EQ(check.normalized, "https://nas.example.com");
}

TEST(ServerUrlTest, AddsHttpsWhenNoSchemeIsGiven) {
    // Wer "nas.example.com" eintippt, meint HTTPS. Das stillschweigend zu
    // ergänzen ist bequem und sicher - die Gegenrichtung wäre es nicht.
    const auto check = checkServerUrl("nas.example.com");
    EXPECT_TRUE(check.ok());
    EXPECT_EQ(check.normalized, "https://nas.example.com");
}

TEST(ServerUrlTest, RejectsEmptyInput) {
    EXPECT_EQ(checkServerUrl("").problem, ServerUrlProblem::Empty);
    EXPECT_EQ(checkServerUrl("    ").problem, ServerUrlProblem::Empty);
    EXPECT_FALSE(checkServerUrl("").ok());
}

TEST(ServerUrlTest, RejectsSchemesOtherThanHttpAndHttps) {
    EXPECT_EQ(checkServerUrl("ftp://nas.example.com").problem,
              ServerUrlProblem::UnsupportedScheme);
    EXPECT_EQ(checkServerUrl("file:///c:/tmp").problem,
              ServerUrlProblem::UnsupportedScheme);
}

TEST(ServerUrlTest, RejectsPlainHttpToARemoteHost) {
    // Der Kern der Regel: über diese Verbindung geht ein Zugangstoken.
    const auto check = checkServerUrl("http://nas.example.com");
    EXPECT_FALSE(check.ok());
    EXPECT_EQ(check.problem, ServerUrlProblem::InsecureScheme);
    EXPECT_FALSE(check.message.empty());
}

TEST(ServerUrlTest, RejectsPlainHttpToARemoteIpAddress) {
    EXPECT_EQ(checkServerUrl("http://192.168.178.53:8000").problem,
              ServerUrlProblem::InsecureScheme);
}

TEST(ServerUrlTest, AllowsPlainHttpToLocalhost) {
    // Ausnahme laut Spec Abschnitt 2.9, damit Entwicklung gegen eine lokale
    // Dev-Instanz möglich bleibt.
    EXPECT_TRUE(checkServerUrl("http://localhost:8000").ok());
    EXPECT_TRUE(checkServerUrl("http://127.0.0.1:8000").ok());
    EXPECT_TRUE(checkServerUrl("http://[::1]:8000").ok());
}

TEST(ServerUrlTest, TreatsLocalhostCaseInsensitively) {
    EXPECT_TRUE(checkServerUrl("http://LOCALHOST:8000").ok());
}

TEST(ServerUrlTest, DoesNotMistakeALookalikeHostForLocalhost) {
    // "localhost.example.com" ist ein fremder Rechner, kein localhost.
    EXPECT_EQ(checkServerUrl("http://localhost.example.com").problem,
              ServerUrlProblem::InsecureScheme);
    EXPECT_EQ(checkServerUrl("http://notlocalhost").problem,
              ServerUrlProblem::InsecureScheme);
    // Auch nicht über die Benutzerangabe hereinzulegen.
    EXPECT_EQ(checkServerUrl("http://localhost@evil.example.com").problem,
              ServerUrlProblem::InsecureScheme);
}

TEST(ServerUrlTest, RejectsAUrlWithoutAHost) {
    EXPECT_EQ(checkServerUrl("https://").problem, ServerUrlProblem::Malformed);
    EXPECT_EQ(checkServerUrl("https:///pfad").problem, ServerUrlProblem::Malformed);
}

TEST(ServerUrlTest, ProducesAMessageForEveryProblem) {
    // Die Meldung landet später im Dialog. Kein Problem ohne Erklärung.
    EXPECT_FALSE(checkServerUrl("").message.empty());
    EXPECT_FALSE(checkServerUrl("https://").message.empty());
    EXPECT_FALSE(checkServerUrl("ftp://x").message.empty());
    EXPECT_FALSE(checkServerUrl("http://nas.example.com").message.empty());
    EXPECT_TRUE(checkServerUrl("https://nas.example.com").message.empty());
}
```

- [ ] **Step 2: `tests/CMakeLists.txt` erweitern**

In der Quelldateiliste von `baludesk_core_tests` ergänzen. Die Datei sonst unverändert lassen — insbesondere bleiben `find_package(Qt6 ...)` und `qt_standard_project_setup()` **vor** beiden `add_executable()`:

```cmake
add_executable(baludesk_core_tests
    core/app_config_test.cpp
    core/logger_test.cpp
    core/tls_trust_test.cpp
    core/credential_store_test.cpp
    core/http_client_test.cpp
    core/server_url_test.cpp
)
```

- [ ] **Step 3: Test laufen lassen und Fehlschlag bestätigen**

```powershell
cmake --preset msvc2022
cmake --build --preset debug
```

Erwartet: FEHLER beim Übersetzen — `Cannot open include file: 'core/api/ServerUrl.h'`. CMake bricht hier noch nicht ab, weil die neue Quelldatei erst in Step 5 in `add_library` aufgenommen wird.

- [ ] **Step 4: `src/core/api/ServerUrl.h` schreiben**

```cpp
#pragma once

#include <string>

namespace baludesk::core {

// Was an einer eingegebenen Serveradresse nicht stimmt.
enum class ServerUrlProblem {
    None,
    // Nichts eingegeben.
    Empty,
    // Kein Rechnername erkennbar.
    Malformed,
    // Etwas anderes als http oder https.
    UnsupportedScheme,
    // Blankes http zu einem fremden Rechner. Über diese Verbindung geht ein
    // Zugangstoken - siehe Spec Abschnitt 2.9.
    InsecureScheme
};

struct ServerUrlCheck {
    ServerUrlProblem problem = ServerUrlProblem::None;

    // Bereinigte Fassung: Leerraum entfernt, fehlendes Schema auf https
    // ergänzt, abschliessende Schrägstriche entfernt. Nur belegt, wenn die
    // Prüfung bestanden wurde.
    std::string normalized;

    // Erklärung im Klartext, zur Anzeige geeignet. Leer, wenn alles stimmt.
    std::string message;

    [[nodiscard]] bool ok() const { return problem == ServerUrlProblem::None; }
};

// Prüft und normalisiert eine eingegebene Serveradresse.
//
// Absichtlich hier und nicht im Eingabedialog: die Regel gilt für jeden
// Aufrufer, und der nächste Einstiegspunkt würde eine Prüfung im Dialog
// umgehen.
//
// Fehlt das Schema, wird https ergänzt. Blankes http ist ausschliesslich zu
// localhost erlaubt, damit Entwicklung gegen eine lokale Dev-Instanz möglich
// bleibt.
[[nodiscard]] ServerUrlCheck checkServerUrl(const std::string& input);

}  // namespace baludesk::core
```

- [ ] **Step 5: `src/core/api/ServerUrl.cpp` schreiben**

```cpp
#include "core/api/ServerUrl.h"

#include <algorithm>
#include <cctype>

namespace baludesk::core {
namespace {

std::string trimmed(const std::string& text) {
    const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };

    auto begin = text.begin();
    while (begin != text.end() && isSpace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    auto end = text.end();
    while (end != begin && isSpace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    return std::string(begin, end);
}

std::string lowered(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

void stripTrailingSlashes(std::string& url) {
    while (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
}

// Der Teil zwischen Schema und erstem Schrägstrich, ohne Benutzerangabe und
// ohne Portnummer. Leer, wenn keiner da ist.
std::string hostOf(const std::string& urlWithoutScheme) {
    const std::size_t pathStart = urlWithoutScheme.find('/');
    std::string authority = urlWithoutScheme.substr(0, pathStart);

    // Eine Benutzerangabe vor dem @ gehört nicht zum Rechnernamen. Ohne
    // diesen Schnitt liesse sich "http://localhost@fremder.host" als
    // localhost ausgeben.
    const std::size_t at = authority.rfind('@');
    if (at != std::string::npos) {
        authority = authority.substr(at + 1);
    }

    if (!authority.empty() && authority.front() == '[') {
        // IPv6 in eckigen Klammern, etwa [::1]:8000
        const std::size_t close = authority.find(']');
        if (close == std::string::npos) {
            return {};
        }
        return authority.substr(0, close + 1);
    }

    const std::size_t colon = authority.find(':');
    if (colon != std::string::npos) {
        authority = authority.substr(0, colon);
    }
    return authority;
}

bool isLocalhost(const std::string& host) {
    const std::string lower = lowered(host);
    return lower == "localhost" || lower == "127.0.0.1" || lower == "[::1]";
}

}  // namespace

ServerUrlCheck checkServerUrl(const std::string& input) {
    ServerUrlCheck check;

    const std::string text = trimmed(input);
    if (text.empty()) {
        check.problem = ServerUrlProblem::Empty;
        check.message = "Bitte eine Serveradresse eingeben.";
        return check;
    }

    std::string url = text;
    std::string scheme;

    const std::size_t schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos) {
        // Ohne Angabe wird https angenommen. Andersherum - stillschweigend auf
        // http zurückfallen - wäre gefährlich.
        scheme = "https";
        url = "https://" + url;
    } else {
        scheme = lowered(url.substr(0, schemeEnd));
        if (scheme != "http" && scheme != "https") {
            check.problem = ServerUrlProblem::UnsupportedScheme;
            check.message = "Nur http und https werden unterstützt, nicht „" + scheme + "“.";
            return check;
        }
        url = scheme + url.substr(schemeEnd);
    }

    const std::string rest = url.substr(scheme.size() + 3);
    const std::string host = hostOf(rest);
    if (host.empty()) {
        check.problem = ServerUrlProblem::Malformed;
        check.message = "In der Adresse steckt kein Rechnername.";
        return check;
    }

    if (scheme == "http" && !isLocalhost(host)) {
        check.problem = ServerUrlProblem::InsecureScheme;
        check.message =
            "Unverschlüsseltes http ist nur zu localhost erlaubt. Über diese "
            "Verbindung geht ein Zugangstoken — bitte https verwenden.";
        return check;
    }

    stripTrailingSlashes(url);
    check.normalized = url;
    return check;
}

}  // namespace baludesk::core
```

- [ ] **Step 6: Die Erzeugerfunktion an `TlsPolicy` ergänzen**

Der Abschlussreview der Stufe 1a merkte an, dass nichts einen Pin an eine Nutzerentscheidung bindet: `TlsPolicy` ist ein offenes Aggregat, und in dieser Stufe kommt der Pin aus einer Datei. Eine benannte Erzeugerfunktion macht an der Aufrufstelle sichtbar, woher der Wert stammen muss.

In `src/core/api/TlsPolicy.h`, innerhalb der Struktur nach dem Feld `publicKeyPin`, ergänzen:

```cpp
    // Erzeugt eine Regel, die gegen den angehefteten Schlüssel prüft.
    //
    // Der Name ist der Zweck: dieser Wert darf ausschließlich aus einem
    // abgeschlossenen Bestätigungsvorgang stammen, in dem ein Mensch den
    // Fingerabdruck gesehen und angenommen hat. Wer ihn anderswoher nimmt,
    // umgeht die Entscheidung, um die es in Spec Abschnitt 2.9 geht.
    //
    // Ein leerer Pin ergibt bewusst wieder SystemTrust statt einer Regel, die
    // nichts prüft.
    [[nodiscard]] static TlsPolicy pinnedFromConfirmedCertificate(const std::string& publicKeyPin) {
        TlsPolicy policy;
        if (publicKeyPin.empty()) {
            return policy;
        }
        policy.mode = Mode::Pinned;
        policy.publicKeyPin = publicKeyPin;
        return policy;
    }
```

- [ ] **Step 7: `src/core/CMakeLists.txt` erweitern**

`api/ServerUrl.cpp` in die Quelldateiliste aufnehmen. Alles Übrige — die `find_package`-Aufrufe, die Verknüpfungen, der `Advapi32`-Block — unverändert lassen:

```cmake
add_library(baludesk_core STATIC
    util/AppConfig.cpp
    util/Logger.cpp
    util/TlsTrust.cpp
    util/CredentialStore.cpp
    util/WindowsCredentialStore.cpp
    api/HttpClient.cpp
    api/CurlTransport.cpp
    api/ServerUrl.cpp
)
```

*Hinweis:* Falls die Liste in der Datei von dieser abweicht, ist die Datei maßgeblich — dann nur die Zeile `api/ServerUrl.cpp` ergänzen und den Rest so lassen, wie er ist.

- [ ] **Step 8: Tests laufen lassen und grün bestätigen**

```powershell
cmake --preset msvc2022
cmake --build --preset debug
ctest --preset debug
```

Erwartet: 74 Tests, alle bestanden (60 vorhandene plus deine 14).

- [ ] **Step 9: Committen**

```powershell
git add src/core tests/core tests/CMakeLists.txt
git commit -m "feat(core): ServerUrl-Prüfung und benannte Pin-Erzeugerfunktion"
```

---

## Task 2: PairingClient — der Gerätecode-Ablauf

**Files:**
- Create: `src/core/auth/PairingTypes.h`
- Create: `src/core/auth/PairingClient.h`
- Create: `src/core/auth/PairingClient.cpp`
- Modify: `src/core/CMakeLists.txt`
- Test: `tests/core/pairing_client_test.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `HttpClient`, `HttpResult`, `HttpErrorKind` aus Stufe 1a
- Produces:
  - `enum class PairingState { Idle, WaitingForApproval, Approved, Denied, Expired, Failed }`
  - `struct PairingPrompt { std::string userCode; std::string verificationUrl; int expiresInSeconds; }`
  - `struct PairingTokens { std::string accessToken; std::string refreshToken; std::string username; }`
  - `struct PairingProgress { PairingState state; int waitSeconds; std::string message; }`
  - `class PairingClient` mit
    `explicit PairingClient(HttpClient& http)`,
    `PairingProgress begin(const std::string& deviceId, const std::string& deviceName, std::int64_t nowSeconds)`,
    `PairingProgress poll(std::int64_t nowSeconds)`,
    `const std::optional<PairingPrompt>& prompt() const`,
    `const std::optional<PairingTokens>& tokens() const`,
    `PairingState state() const`

---

- [ ] **Step 1: Den fehlschlagenden Test schreiben**

Der Automat bekommt die Zeit hereingereicht und schläft nie selbst. Dadurch ist das gesamte Zeitverhalten prüfbar, ohne dass ein Test auch nur eine Sekunde wartet — und die Oberfläche in Stufe 1c kann `poll` an einen `QTimer` hängen, statt einen Thread zu blockieren.

Datei `tests/core/pairing_client_test.cpp`:

```cpp
#include "core/auth/PairingClient.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "core/api/HttpClient.h"
#include "support/FakeTransport.h"

using baludesk::core::HttpClient;
using baludesk::core::HttpErrorKind;
using baludesk::core::PairingClient;
using baludesk::core::PairingState;
using baludesk::core::TlsPolicy;
using baludesk::core::TransportResult;
using baludesk::core::test::FakeTransport;

namespace {

TransportResult jsonResult(long statusCode, std::string body) {
    TransportResult result;
    result.error = HttpErrorKind::None;
    result.response.statusCode = statusCode;
    result.response.body = std::move(body);
    return result;
}

TransportResult transportFailure(HttpErrorKind kind, std::string detail) {
    TransportResult result;
    result.error = kind;
    result.detail = std::move(detail);
    return result;
}

const char* kDeviceCodeResponse = R"({
  "device_code": "geheimer-geraetecode",
  "user_code": "483920",
  "verification_url": "https://nas.example.com/devices?pair=1",
  "expires_in": 600,
  "interval": 5
})";

const char* kPendingResponse = R"({"status":"authorization_pending"})";

const char* kApprovedResponse = R"({
  "status": "approved",
  "access_token": "zugangstoken",
  "refresh_token": "erneuerungstoken",
  "token_type": "bearer",
  "user": {"id": 1, "username": "sven", "email": "sven@example.com", "role": "admin"}
})";

struct Fixture {
    std::shared_ptr<FakeTransport> transport = std::make_shared<FakeTransport>();
    std::unique_ptr<HttpClient> http;
    std::unique_ptr<PairingClient> client;

    Fixture() {
        http = std::make_unique<HttpClient>("https://nas.example.com", transport, TlsPolicy{});
        client = std::make_unique<PairingClient>(*http);
    }
};

}  // namespace

TEST(PairingClientTest, StartsIdle) {
    Fixture f;
    EXPECT_EQ(f.client->state(), PairingState::Idle);
    EXPECT_FALSE(f.client->prompt().has_value());
    EXPECT_FALSE(f.client->tokens().has_value());
}

TEST(PairingClientTest, BeginRequestsACodeAndExposesThePrompt) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));

    const auto progress = f.client->begin("geraet-1", "Svens Rechner", 1000);

    EXPECT_EQ(progress.state, PairingState::WaitingForApproval);
    ASSERT_TRUE(f.client->prompt().has_value());
    EXPECT_EQ(f.client->prompt()->userCode, "483920");
    EXPECT_EQ(f.client->prompt()->verificationUrl, "https://nas.example.com/devices?pair=1");
    EXPECT_EQ(f.client->prompt()->expiresInSeconds, 600);
}

TEST(PairingClientTest, BeginSendsTheExpectedRequest) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));

    f.client->begin("geraet-1", "Svens Rechner", 1000);

    ASSERT_EQ(f.transport->calls.size(), std::size_t{1});
    const auto& call = f.transport->calls[0];
    EXPECT_EQ(call.url, "https://nas.example.com/api/desktop-pairing/device-code");
    EXPECT_NE(call.request.jsonBody.find("\"device_id\":\"geraet-1\""), std::string::npos);
    EXPECT_NE(call.request.jsonBody.find("\"device_name\":\"Svens Rechner\""), std::string::npos);
    // Der Server prüft platform gegen ^(windows|mac|linux)$.
    EXPECT_NE(call.request.jsonBody.find("\"platform\":\"windows\""), std::string::npos);
    // Pairing läuft ohne Anmeldung.
    EXPECT_TRUE(call.request.bearerToken.empty());
}

TEST(PairingClientTest, HonoursTheIntervalTheServerAsksFor) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));

    const auto progress = f.client->begin("geraet-1", "Rechner", 1000);

    // Der Server gibt interval=5 vor, und bei 12 Anfragen je Minute ist das
    // exakt ein Abruf je Intervall - ohne jede Reserve. Schneller zu fragen
    // sperrt den Client aus.
    EXPECT_EQ(progress.waitSeconds, 5);
}

TEST(PairingClientTest, StaysWaitingWhileTheServerReportsPending) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.client->begin("geraet-1", "Rechner", 1000);

    f.transport->queueResult(jsonResult(200, kPendingResponse));
    const auto progress = f.client->poll(1005);

    EXPECT_EQ(progress.state, PairingState::WaitingForApproval);
    EXPECT_EQ(progress.waitSeconds, 5);
    EXPECT_FALSE(f.client->tokens().has_value());
}

TEST(PairingClientTest, PollSendsTheDeviceCodeBack) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.client->begin("geraet-1", "Rechner", 1000);

    f.transport->queueResult(jsonResult(200, kPendingResponse));
    f.client->poll(1005);

    ASSERT_EQ(f.transport->calls.size(), std::size_t{2});
    const auto& call = f.transport->calls[1];
    EXPECT_EQ(call.url, "https://nas.example.com/api/desktop-pairing/poll");
    EXPECT_NE(call.request.jsonBody.find("geheimer-geraetecode"), std::string::npos);
}

TEST(PairingClientTest, CapturesTheTokensOnApproval) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.client->begin("geraet-1", "Rechner", 1000);

    f.transport->queueResult(jsonResult(200, kApprovedResponse));
    const auto progress = f.client->poll(1005);

    EXPECT_EQ(progress.state, PairingState::Approved);
    ASSERT_TRUE(f.client->tokens().has_value());
    EXPECT_EQ(f.client->tokens()->accessToken, "zugangstoken");
    EXPECT_EQ(f.client->tokens()->refreshToken, "erneuerungstoken");
    EXPECT_EQ(f.client->tokens()->username, "sven");
}

TEST(PairingClientTest, DoesNotPollAgainAfterApproval) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.client->begin("geraet-1", "Rechner", 1000);
    f.transport->queueResult(jsonResult(200, kApprovedResponse));
    f.client->poll(1005);

    const std::size_t callsAfterApproval = f.transport->calls.size();
    const auto progress = f.client->poll(1010);

    // Der Server löscht den Datensatz bei der Auslieferung. Ein zweiter
    // Abruf brachte nur ein 404 und würde den erreichten Zustand zerstören.
    EXPECT_EQ(progress.state, PairingState::Approved);
    EXPECT_EQ(f.transport->calls.size(), callsAfterApproval);
    ASSERT_TRUE(f.client->tokens().has_value());
    EXPECT_EQ(f.client->tokens()->accessToken, "zugangstoken");
}

TEST(PairingClientTest, ReportsDenial) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.client->begin("geraet-1", "Rechner", 1000);

    f.transport->queueResult(jsonResult(200, R"({"status":"denied"})"));
    const auto progress = f.client->poll(1005);

    EXPECT_EQ(progress.state, PairingState::Denied);
    EXPECT_FALSE(progress.message.empty());
    EXPECT_FALSE(f.client->tokens().has_value());
}

TEST(PairingClientTest, ReportsExpiryFromTheServer) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.client->begin("geraet-1", "Rechner", 1000);

    f.transport->queueResult(jsonResult(200, R"({"status":"expired"})"));
    const auto progress = f.client->poll(1005);

    EXPECT_EQ(progress.state, PairingState::Expired);
}

TEST(PairingClientTest, StopsOnItsOwnWhenTheCodeLifetimeRunsOut) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.client->begin("geraet-1", "Rechner", 1000);

    // expires_in war 600. Nach 601 Sekunden ist nichts mehr zu holen -
    // weiterzufragen verbraucht nur Kontingent.
    const std::size_t callsBefore = f.transport->calls.size();
    const auto progress = f.client->poll(1601);

    EXPECT_EQ(progress.state, PairingState::Expired);
    EXPECT_EQ(f.transport->calls.size(), callsBefore);
}

TEST(PairingClientTest, TreatsAMissingPairingRecordAsExpired) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.client->begin("geraet-1", "Rechner", 1000);

    // 404 "Invalid device code" - der Datensatz ist weg. Für den Nutzer ist
    // das dasselbe wie abgelaufen.
    f.transport->queueResult(jsonResult(404, R"({"detail":"Invalid device code"})"));
    const auto progress = f.client->poll(1005);

    EXPECT_EQ(progress.state, PairingState::Expired);
}

TEST(PairingClientTest, BacksOffWhenTheServerReportsTooManyRequests) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.client->begin("geraet-1", "Rechner", 1000);

    f.transport->queueResult(jsonResult(
        429, R"({"error":"Too Many Requests","detail":"Rate limit exceeded.","retry_after":37})"));
    const auto progress = f.client->poll(1005);

    // Der Ablauf bricht nicht ab, er wartet länger - und zwar so lange, wie
    // der Server im Rumpf angibt. Kopfzeilen trägt HttpResponse nicht.
    EXPECT_EQ(progress.state, PairingState::WaitingForApproval);
    EXPECT_EQ(progress.waitSeconds, 37);
}

TEST(PairingClientTest, FallsBackToASaneDelayWhenRetryAfterIsMissing) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.client->begin("geraet-1", "Rechner", 1000);

    f.transport->queueResult(jsonResult(429, R"({"error":"Too Many Requests"})"));
    const auto progress = f.client->poll(1005);

    EXPECT_EQ(progress.state, PairingState::WaitingForApproval);
    EXPECT_GE(progress.waitSeconds, 30);
}

TEST(PairingClientTest, KeepsWaitingThroughATransientNetworkError) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.client->begin("geraet-1", "Rechner", 1000);

    f.transport->queueResult(transportFailure(HttpErrorKind::Network, "connection refused"));
    const auto progress = f.client->poll(1005);

    // Ein schlafender oder gerade neu startender NAS ist kein Grund, die
    // Paarung wegzuwerfen - der Code lebt zehn Minuten.
    EXPECT_EQ(progress.state, PairingState::WaitingForApproval);
    EXPECT_GT(progress.waitSeconds, 0);
}

TEST(PairingClientTest, GivesUpOnAPinMismatch) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.client->begin("geraet-1", "Rechner", 1000);

    f.transport->queueResult(
        transportFailure(HttpErrorKind::CertificatePinMismatch, "pinned key mismatch"));
    const auto progress = f.client->poll(1005);

    // Spec Abschnitt 2.9: bei einer Pin-Abweichung wird nicht nachgefragt und
    // nicht weitergemacht, sondern hart abgebrochen.
    EXPECT_EQ(progress.state, PairingState::Failed);
    EXPECT_FALSE(progress.message.empty());
}

TEST(PairingClientTest, FailsWhenTheServerRefusesToIssueACode) {
    Fixture f;
    f.transport->queueResult(jsonResult(500, R"({"detail":"boom"})"));

    const auto progress = f.client->begin("geraet-1", "Rechner", 1000);

    EXPECT_EQ(progress.state, PairingState::Failed);
    EXPECT_FALSE(f.client->prompt().has_value());
}

TEST(PairingClientTest, FailsOnAnUnreadableCodeResponse) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, "kein json"));

    const auto progress = f.client->begin("geraet-1", "Rechner", 1000);

    EXPECT_EQ(progress.state, PairingState::Failed);
}

TEST(PairingClientTest, FailsOnAnApprovalWithoutTokens) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.client->begin("geraet-1", "Rechner", 1000);

    // Freigegeben, aber ohne Token - damit ist nichts anzufangen, und weil
    // der Server den Datensatz gelöscht hat, hilft auch kein zweiter Versuch.
    f.transport->queueResult(jsonResult(200, R"({"status":"approved"})"));
    const auto progress = f.client->poll(1005);

    EXPECT_EQ(progress.state, PairingState::Failed);
    EXPECT_FALSE(f.client->tokens().has_value());
}

TEST(PairingClientTest, PollBeforeBeginIsRefusedWithoutTouchingTheNetwork) {
    Fixture f;

    const auto progress = f.client->poll(1000);

    EXPECT_EQ(progress.state, PairingState::Idle);
    EXPECT_TRUE(f.transport->calls.empty());
}
```

- [ ] **Step 2: `tests/CMakeLists.txt` erweitern**

```cmake
add_executable(baludesk_core_tests
    core/app_config_test.cpp
    core/logger_test.cpp
    core/tls_trust_test.cpp
    core/credential_store_test.cpp
    core/http_client_test.cpp
    core/server_url_test.cpp
    core/pairing_client_test.cpp
)
```

Damit `#include "support/FakeTransport.h"` aus `core/` heraus auflöst, muss das Testverzeichnis im Suchpfad liegen. Prüfe, ob `baludesk_core_tests` bereits ein `target_include_directories(... ${CMAKE_CURRENT_SOURCE_DIR})` besitzt; falls nicht, ergänze es — der vorhandene Test bindet die Datei möglicherweise über einen relativen Pfad ein, und dieser Plan nutzt bewusst `support/FakeTransport.h`.

- [ ] **Step 3: Test laufen lassen und Fehlschlag bestätigen**

```powershell
cmake --preset msvc2022
cmake --build --preset debug
```

Erwartet: FEHLER beim Übersetzen — `Cannot open include file: 'core/auth/PairingClient.h'`.

- [ ] **Step 4: `src/core/auth/PairingTypes.h` schreiben**

```cpp
#pragma once

#include <string>

namespace baludesk::core {

// Wo der Paarungsvorgang gerade steht.
enum class PairingState {
    // Noch nicht begonnen.
    Idle,
    // Code liegt vor, es wird auf die Freigabe im Browser gewartet.
    WaitingForApproval,
    // Freigegeben, Token liegen vor.
    Approved,
    // Im Browser abgelehnt.
    Denied,
    // Der Code ist verfallen oder der Server kennt ihn nicht mehr.
    Expired,
    // Endgültig gescheitert - siehe Meldung.
    Failed
};

// Was dem Nutzer gezeigt werden muss, damit er die Paarung freigeben kann.
struct PairingPrompt {
    // Sechsstellig, wird im Browser eingegeben.
    std::string userCode;
    // Adresse, an der die Freigabe stattfindet.
    std::string verificationUrl;
    // Restlaufzeit des Codes ab Ausstellung.
    int expiresInSeconds = 0;
};

// Das Ergebnis einer erfolgreichen Paarung.
//
// ACHTUNG: Der Server liefert diese Werte genau einmal aus und löscht dabei
// seinen Datensatz. Ein erneuter Abruf mit demselben Gerätecode liefert 404.
// Wer sie verliert, muss neu paaren - sie gehören deshalb abgelegt, bevor
// irgendetwas anderes geschieht.
struct PairingTokens {
    std::string accessToken;
    std::string refreshToken;
    std::string username;
};

// Was nach einem Schritt zu tun ist.
struct PairingProgress {
    PairingState state = PairingState::Idle;

    // Sekunden bis zum nächsten poll(). 0, wenn nicht mehr zu fragen ist.
    int waitSeconds = 0;

    // Erklärung im Klartext, zur Anzeige geeignet. Leer, solange alles
    // seinen normalen Gang geht.
    std::string message;
};

}  // namespace baludesk::core
```

- [ ] **Step 5: `src/core/auth/PairingClient.h` schreiben**

```cpp
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "core/auth/PairingTypes.h"

namespace baludesk::core {

class HttpClient;

// Der Gerätecode-Ablauf als Zustandsautomat.
//
// Die Klasse schläft nie und kennt die Uhr nicht: die aktuelle Zeit wird ihr
// hereingereicht, und sie gibt zurück, nach wie vielen Sekunden sie wieder
// gefragt werden möchte. Dadurch lässt sich das gesamte Zeitverhalten prüfen,
// ohne dass ein Test auch nur eine Sekunde wartet, und die Oberfläche kann
// poll() an einen Zeitgeber hängen statt einen Thread zu blockieren.
//
// Das vom Server gelieferte Intervall ist einzuhalten und nicht zu
// unterbieten: bei 12 zugelassenen Abrufen je Minute und einem vorgegebenen
// Intervall von 5 Sekunden gibt es keine Reserve.
class PairingClient {
public:
    explicit PairingClient(HttpClient& http);

    // Fordert einen Gerätecode an. Danach steht prompt() bereit.
    // nowSeconds ist eine beliebige, monoton wachsende Sekundenzählung.
    PairingProgress begin(const std::string& deviceId,
                          const std::string& deviceName,
                          std::int64_t nowSeconds);

    // Fragt einmal nach dem Stand. Erst aufrufen, wenn seit dem letzten
    // Ergebnis so viele Sekunden vergangen sind, wie es angab.
    //
    // Nach einem Endzustand - Approved, Denied, Expired, Failed - werden
    // keine Anfragen mehr gestellt; der erreichte Zustand wird unverändert
    // zurückgegeben. Das ist wichtig, weil ein zweiter Abruf nach der
    // Freigabe nur ein 404 brächte.
    PairingProgress poll(std::int64_t nowSeconds);

    [[nodiscard]] const std::optional<PairingPrompt>& prompt() const { return prompt_; }
    [[nodiscard]] const std::optional<PairingTokens>& tokens() const { return tokens_; }
    [[nodiscard]] PairingState state() const { return state_; }

private:
    PairingProgress finish(PairingState state, std::string message);
    PairingProgress waiting(int waitSeconds, std::string message = {});

    HttpClient& http_;

    PairingState state_ = PairingState::Idle;
    std::optional<PairingPrompt> prompt_;
    std::optional<PairingTokens> tokens_;

    std::string deviceCode_;
    int intervalSeconds_ = 5;
    std::int64_t deadlineSeconds_ = 0;
};

}  // namespace baludesk::core
```

- [ ] **Step 6: `src/core/auth/PairingClient.cpp` schreiben**

```cpp
#include "core/auth/PairingClient.h"

#include <nlohmann/json.hpp>

#include <utility>

#include "core/api/HttpClient.h"
#include "core/util/Logger.h"

namespace baludesk::core {
namespace {

constexpr const char* kDeviceCodePath = "/api/desktop-pairing/device-code";
constexpr const char* kPollPath = "/api/desktop-pairing/poll";

// Wenn der Server bei 429 keinen Wert nennt, lieber deutlich zu lange warten
// als sich erneut auszusperren.
constexpr int kFallbackBackoffSeconds = 60;

// Ein Netzwerkfehler ist kein Grund aufzugeben - der Code lebt zehn Minuten,
// und ein NAS kann in der Zeit durchaus neu starten.
constexpr int kNetworkRetrySeconds = 10;

std::optional<nlohmann::json> parse(const std::string& body) {
    auto document = nlohmann::json::parse(body, nullptr, false);
    if (document.is_discarded() || !document.is_object()) {
        return std::nullopt;
    }
    return document;
}

std::string stringField(const nlohmann::json& document, const char* name) {
    const auto it = document.find(name);
    if (it == document.end() || !it->is_string()) {
        return {};
    }
    return it->get<std::string>();
}

int intField(const nlohmann::json& document, const char* name, int fallback) {
    const auto it = document.find(name);
    if (it == document.end() || !it->is_number_integer()) {
        return fallback;
    }
    return it->get<int>();
}

}  // namespace

PairingClient::PairingClient(HttpClient& http) : http_(http) {}

PairingProgress PairingClient::finish(PairingState state, std::string message) {
    state_ = state;
    PairingProgress progress;
    progress.state = state;
    progress.waitSeconds = 0;
    progress.message = std::move(message);
    return progress;
}

PairingProgress PairingClient::waiting(int waitSeconds, std::string message) {
    state_ = PairingState::WaitingForApproval;
    PairingProgress progress;
    progress.state = PairingState::WaitingForApproval;
    progress.waitSeconds = waitSeconds;
    progress.message = std::move(message);
    return progress;
}

PairingProgress PairingClient::begin(const std::string& deviceId,
                                     const std::string& deviceName,
                                     std::int64_t nowSeconds) {
    nlohmann::json request;
    request["device_id"] = deviceId;
    request["device_name"] = deviceName;
    // Der Server prüft gegen ^(windows|mac|linux)$. Stufe 1a wird nur unter
    // Windows gebaut; wenn weitere Plattformen dazukommen, gehört hier eine
    // Fallunterscheidung hin.
    request["platform"] = "windows";

    const HttpResult result = http_.postJson(kDeviceCodePath, request.dump());

    if (result.error == HttpErrorKind::CertificatePinMismatch) {
        Logger::error("Pairing: Zertifikat weicht vom angehefteten Schlüssel ab");
        return finish(PairingState::Failed,
                      "Das Zertifikat des Servers weicht vom bestätigten ab. "
                      "Die Verbindung wurde abgebrochen.");
    }
    if (!result.ok()) {
        Logger::warn("Pairing: Gerätecode nicht erhalten, Status {} — {}",
                     result.statusCode, result.detail);
        return finish(PairingState::Failed,
                      "Der Server hat keinen Gerätecode ausgestellt.");
    }

    const auto document = parse(result.body);
    if (!document) {
        Logger::error("Pairing: Antwort auf die Codeanforderung war kein JSON-Objekt");
        return finish(PairingState::Failed, "Die Antwort des Servers war unverständlich.");
    }

    const std::string deviceCode = stringField(*document, "device_code");
    const std::string userCode = stringField(*document, "user_code");
    if (deviceCode.empty() || userCode.empty()) {
        Logger::error("Pairing: Antwort ohne device_code oder user_code");
        return finish(PairingState::Failed, "Die Antwort des Servers war unvollständig.");
    }

    deviceCode_ = deviceCode;
    intervalSeconds_ = intField(*document, "interval", 5);
    if (intervalSeconds_ < 1) {
        intervalSeconds_ = 5;
    }

    PairingPrompt readyPrompt;
    readyPrompt.userCode = userCode;
    readyPrompt.verificationUrl = stringField(*document, "verification_url");
    readyPrompt.expiresInSeconds = intField(*document, "expires_in", 600);
    prompt_ = readyPrompt;

    deadlineSeconds_ = nowSeconds + readyPrompt.expiresInSeconds;

    Logger::info("Pairing: Code ausgestellt, gültig {} s, Intervall {} s",
                 readyPrompt.expiresInSeconds, intervalSeconds_);

    return waiting(intervalSeconds_);
}

PairingProgress PairingClient::poll(std::int64_t nowSeconds) {
    if (state_ == PairingState::Idle) {
        PairingProgress progress;
        progress.state = PairingState::Idle;
        progress.message = "Es läuft keine Paarung.";
        return progress;
    }
    if (state_ != PairingState::WaitingForApproval) {
        // Endzustand. Nicht erneut fragen - nach der Freigabe hat der Server
        // seinen Datensatz gelöscht, ein weiterer Abruf brächte nur 404.
        PairingProgress progress;
        progress.state = state_;
        return progress;
    }

    if (nowSeconds >= deadlineSeconds_) {
        Logger::info("Pairing: Gültigkeit des Codes abgelaufen");
        return finish(PairingState::Expired,
                      "Der Code ist abgelaufen. Bitte die Paarung neu starten.");
    }

    nlohmann::json request;
    request["device_code"] = deviceCode_;

    const HttpResult result = http_.postJson(kPollPath, request.dump());

    if (result.error == HttpErrorKind::CertificatePinMismatch) {
        Logger::error("Pairing: Zertifikat weicht vom angehefteten Schlüssel ab");
        return finish(PairingState::Failed,
                      "Das Zertifikat des Servers weicht vom bestätigten ab. "
                      "Die Verbindung wurde abgebrochen.");
    }
    if (result.error != HttpErrorKind::None) {
        // Netz weg, Zeitlimit, Zertifikat noch nicht bestätigt: alles
        // vorübergehend. Später erneut versuchen, solange der Code lebt.
        Logger::debug("Pairing: Abruf fehlgeschlagen ({}), neuer Versuch in {} s",
                      result.detail, kNetworkRetrySeconds);
        return waiting(kNetworkRetrySeconds);
    }

    if (result.statusCode == 429) {
        int retryAfter = kFallbackBackoffSeconds;
        if (const auto document = parse(result.body)) {
            retryAfter = intField(*document, "retry_after", kFallbackBackoffSeconds);
        }
        if (retryAfter < 1) {
            retryAfter = kFallbackBackoffSeconds;
        }
        Logger::warn("Pairing: Ratenbegrenzung erreicht, warte {} s", retryAfter);
        return waiting(retryAfter, "Der Server bremst die Abfragen. Es geht gleich weiter.");
    }

    if (result.statusCode == 404) {
        // "Invalid device code" - der Datensatz existiert nicht mehr.
        Logger::info("Pairing: Server kennt den Gerätecode nicht mehr");
        return finish(PairingState::Expired,
                      "Der Code ist nicht mehr gültig. Bitte die Paarung neu starten.");
    }

    if (!result.ok()) {
        Logger::warn("Pairing: unerwarteter Status {} beim Abruf", result.statusCode);
        return waiting(intervalSeconds_);
    }

    const auto document = parse(result.body);
    if (!document) {
        Logger::error("Pairing: Abrufantwort war kein JSON-Objekt");
        return finish(PairingState::Failed, "Die Antwort des Servers war unverständlich.");
    }

    const std::string status = stringField(*document, "status");

    if (status == "authorization_pending") {
        return waiting(intervalSeconds_);
    }
    if (status == "denied") {
        Logger::info("Pairing: im Browser abgelehnt");
        return finish(PairingState::Denied, "Die Paarung wurde abgelehnt.");
    }
    if (status == "expired") {
        Logger::info("Pairing: vom Server als abgelaufen gemeldet");
        return finish(PairingState::Expired,
                      "Der Code ist abgelaufen. Bitte die Paarung neu starten.");
    }
    if (status == "approved") {
        PairingTokens received;
        received.accessToken = stringField(*document, "access_token");
        received.refreshToken = stringField(*document, "refresh_token");

        const auto user = document->find("user");
        if (user != document->end() && user->is_object()) {
            received.username = stringField(*user, "username");
        }

        if (received.accessToken.empty() || received.refreshToken.empty()) {
            // Freigegeben, aber ohne Token. Der Server hat seinen Datensatz
            // dabei gelöscht - ein zweiter Versuch hilft nicht.
            Logger::error("Pairing: Freigabe ohne verwertbare Token erhalten");
            return finish(PairingState::Failed,
                          "Der Server hat die Paarung freigegeben, aber keine Zugangsdaten "
                          "geliefert. Bitte die Paarung neu starten.");
        }

        tokens_ = received;
        Logger::info("Pairing: freigegeben für Benutzer {}", received.username);
        return finish(PairingState::Approved, {});
    }

    Logger::warn("Pairing: unbekannter Status „{}“", status);
    return waiting(intervalSeconds_);
}

}  // namespace baludesk::core
```

- [ ] **Step 7: `src/core/CMakeLists.txt` erweitern**

`nlohmann_json` wird jetzt gebraucht. `find_package(nlohmann_json CONFIG REQUIRED)` zu den übrigen `find_package`-Aufrufen ergänzen, `auth/PairingClient.cpp` in die Quelldateiliste, und `nlohmann_json::nlohmann_json` bei den privaten Verknüpfungen aufnehmen — privat, weil kein öffentlicher Header von `core/` einen JSON-Typ zeigt:

```cmake
find_package(nlohmann_json CONFIG REQUIRED)

add_library(baludesk_core STATIC
    util/AppConfig.cpp
    util/Logger.cpp
    util/TlsTrust.cpp
    util/CredentialStore.cpp
    util/WindowsCredentialStore.cpp
    api/HttpClient.cpp
    api/CurlTransport.cpp
    api/ServerUrl.cpp
    auth/PairingClient.cpp
)

target_link_libraries(baludesk_core PRIVATE nlohmann_json::nlohmann_json)
```

Die vorhandenen `target_link_libraries`-Aufrufe nicht ersetzen, sondern diesen ergänzen.

- [ ] **Step 8: Tests laufen lassen und grün bestätigen**

```powershell
cmake --preset msvc2022
cmake --build --preset debug
ctest --preset debug
```

Erwartet: 94 Tests, alle bestanden (74 plus deine 20).

- [ ] **Step 9: Committen**

```powershell
git add src/core tests/core tests/CMakeLists.txt
git commit -m "feat(core): PairingClient als zeitloser Zustandsautomat"
```

---

## Task 3: Sitzung ablegen und wiederherstellen

**Files:**
- Create: `src/core/auth/Session.h`
- Create: `src/core/auth/SessionRepository.h`
- Create: `src/core/auth/FileSessionRepository.h`
- Create: `src/core/auth/FileSessionRepository.cpp`
- Modify: `src/core/CMakeLists.txt`
- Create: `tests/core/support/FakeSecretStore.h`
- Test: `tests/core/session_repository_test.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `SecretStore` aus Stufe 1a; `PairingTokens` aus Task 2
- Produces:
  - `struct Session { std::string serverUrl; std::string username; std::string publicKeyPin; std::string accessToken; std::string refreshToken; bool isValid() const; }`
  - `class SessionRepository` mit `virtual std::optional<Session> load()`, `virtual bool save(const Session&)`, `virtual void clear()`
  - `class FileSessionRepository : public SessionRepository` mit `FileSessionRepository(std::string jsonPath, SecretStore& secrets)`

---

- [ ] **Step 1: Den Fake für den Anmeldeinformationsspeicher schreiben**

Die Repository-Tests dürfen nicht in den echten Windows-Speicher schreiben — Stufe 1a hat dafür extra die `SecretStore`-Schnittstelle eingezogen.

Datei `tests/core/support/FakeSecretStore.h`:

```cpp
#pragma once

#include <map>
#include <optional>
#include <string>

#include "core/util/SecretStore.h"

namespace baludesk::core::test {

// Geheimnisse im Arbeitsspeicher statt im Anmeldeinformationsspeicher des
// Betriebssystems. Damit lassen sich Sitzungstests fahren, ohne die echten
// Zugangsdaten des Benutzers anzufassen.
class FakeSecretStore : public SecretStore {
public:
    bool save(const std::string& key, const std::string& secret) override {
        if (key.empty() || failNextSave) {
            failNextSave = false;
            return false;
        }
        entries[key] = secret;
        return true;
    }

    [[nodiscard]] std::optional<std::string> load(const std::string& key) override {
        const auto it = entries.find(key);
        if (it == entries.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    bool erase(const std::string& key) override {
        entries.erase(key);
        return true;
    }

    [[nodiscard]] bool contains(const std::string& key) override {
        return entries.find(key) != entries.end();
    }

    std::map<std::string, std::string> entries;

    // Lässt den nächsten save()-Aufruf scheitern, um die Fehlerbehandlung zu
    // prüfen. Setzt sich danach selbst zurück.
    bool failNextSave = false;
};

}  // namespace baludesk::core::test
```

- [ ] **Step 2: Den fehlschlagenden Test schreiben**

Datei `tests/core/session_repository_test.cpp`:

```cpp
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
```

- [ ] **Step 3: `tests/CMakeLists.txt` erweitern**

```cmake
add_executable(baludesk_core_tests
    core/app_config_test.cpp
    core/logger_test.cpp
    core/tls_trust_test.cpp
    core/credential_store_test.cpp
    core/http_client_test.cpp
    core/server_url_test.cpp
    core/pairing_client_test.cpp
    core/session_repository_test.cpp
)
```

- [ ] **Step 4: Test laufen lassen und Fehlschlag bestätigen**

```powershell
cmake --preset msvc2022
cmake --build --preset debug
```

Erwartet: FEHLER beim Übersetzen — `Cannot open include file: 'core/auth/FileSessionRepository.h'`.

- [ ] **Step 5: `src/core/auth/Session.h` schreiben**

```cpp
#pragma once

#include <string>

namespace baludesk::core {

// Eine bestehende Bindung an einen Server.
//
// Die Bestandteile liegen an zwei Orten: Adresse, Benutzername und der
// bestätigte Zertifikat-Pin sind keine Geheimnisse und stehen in einer
// JSON-Datei; die beiden Token gehören in den Anmeldeinformationsspeicher des
// Betriebssystems. Zusammengesetzt wird das vom SessionRepository.
struct Session {
    // Normalisiert, ohne abschließenden Schrägstrich - siehe checkServerUrl().
    std::string serverUrl;

    // Für die Anzeige „verbunden mit … als …".
    std::string username;

    // Leer, wenn das Zertifikat des Servers regulär geprüft werden kann.
    // Andernfalls der beim Paaren bestätigte Schlüssel-Pin.
    std::string publicKeyPin;

    std::string accessToken;
    std::string refreshToken;

    // Ohne Adresse und ohne beide Token ist nichts anzufangen. Der Pin darf
    // fehlen - bei einem gültigen Zertifikat gibt es keinen.
    [[nodiscard]] bool isValid() const {
        return !serverUrl.empty() && !accessToken.empty() && !refreshToken.empty();
    }
};

}  // namespace baludesk::core
```

- [ ] **Step 6: `src/core/auth/SessionRepository.h` schreiben**

```cpp
#pragma once

#include <optional>

#include "core/auth/Session.h"

namespace baludesk::core {

// Ablage der Sitzung, austauschbar gemacht.
//
// Die Naht existiert aus demselben Grund wie Transport: damit die Logik
// darüber gegen einen Fake geprüft werden kann, statt bei jedem Testlauf
// echte Zugangsdaten des Benutzers anzufassen.
class SessionRepository {
public:
    virtual ~SessionRepository() = default;

    // Nichts, wenn keine vollständige Sitzung vorliegt. Eine halbe Sitzung -
    // Datei ohne Geheimnisse oder umgekehrt - gilt als keine.
    [[nodiscard]] virtual std::optional<Session> load() = 0;

    // Legt alle Bestandteile ab. Schlägt fehl, wenn auch nur einer nicht
    // gespeichert werden konnte; ein halb abgelegter Zustand wäre schlimmer
    // als gar keiner, weil der Server die Token kein zweites Mal ausliefert.
    virtual bool save(const Session& session) = 0;

    // Entfernt alle Bestandteile. Auf einer leeren Ablage unschädlich.
    virtual void clear() = 0;
};

}  // namespace baludesk::core
```

- [ ] **Step 7: `src/core/auth/FileSessionRepository.h` und `.cpp` schreiben**

`src/core/auth/FileSessionRepository.h`:

```cpp
#pragma once

#include <string>

#include "core/auth/SessionRepository.h"

namespace baludesk::core {

class SecretStore;

// Sitzung in einer JSON-Datei plus dem Anmeldeinformationsspeicher.
//
// Bewusst ohne QSettings: das Prüfprogramm muss dieselbe Ablage benutzen wie
// später die Oberfläche, sonst beweist die Abnahme nichts über den echten
// Betrieb - und es ist Qt-frei. Die Bedienvorlieben in AppConfig bleiben
// davon unberührt, die gehören der Oberfläche.
class FileSessionRepository : public SessionRepository {
public:
    FileSessionRepository(std::string jsonPath, SecretStore& secrets);

    [[nodiscard]] std::optional<Session> load() override;
    bool save(const Session& session) override;
    void clear() override;

private:
    std::string jsonPath_;
    SecretStore& secrets_;
};

}  // namespace baludesk::core
```

`src/core/auth/FileSessionRepository.cpp`:

```cpp
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
```

- [ ] **Step 8: `src/core/CMakeLists.txt` erweitern**

`auth/FileSessionRepository.cpp` in die Quelldateiliste aufnehmen:

```cmake
    auth/PairingClient.cpp
    auth/FileSessionRepository.cpp
```

- [ ] **Step 9: Tests laufen lassen und grün bestätigen**

```powershell
cmake --preset msvc2022
cmake --build --preset debug
ctest --preset debug
```

Erwartet: 107 Tests, alle bestanden (94 plus deine 13).

- [ ] **Step 10: Committen**

```powershell
git add src/core tests/core tests/CMakeLists.txt
git commit -m "feat(core): Sitzung in Datei und Anmeldeinformationsspeicher"
```

---

## Task 4: Prüfprogramm zum Paaren erweitern und abnehmen

**Files:**
- Modify: `tools/probe/main.cpp`
- Modify: `tools/probe/CMakeLists.txt`

**Interfaces:**
- Consumes: alles aus Task 1 bis 3 sowie `HttpClient`, `CurlTransport`, `identifyCertificate`, `WindowsCredentialStore`, `Logger` aus Stufe 1a
- Produces: keine für spätere Tasks — dies ist die Abnahme

---

- [ ] **Step 1: `tools/probe/main.cpp` ersetzen**

Aus dem einzelnen Erreichbarkeitstest wird ein kleines Werkzeug mit drei Unterbefehlen. Es fährt denselben Code, den in Stufe 1c die Dialoge fahren werden — deshalb beweist eine erfolgreiche Paarung hier etwas über den echten Betrieb.

```cpp
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "core/api/CurlTransport.h"
#include "core/api/HttpClient.h"
#include "core/api/ServerUrl.h"
#include "core/api/TlsPolicy.h"
#include "core/auth/FileSessionRepository.h"
#include "core/auth/PairingClient.h"
#include "core/util/Logger.h"
#include "core/util/TlsTrust.h"
#include "core/util/WindowsCredentialStore.h"

namespace {

using namespace baludesk::core;

std::int64_t nowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::string sessionPath() {
    const char* localAppData = std::getenv("LOCALAPPDATA");
    const std::string base = localAppData != nullptr ? localAppData : ".";
    return base + "/BaluHost/BaluDesk/session.json";
}

void printUsage() {
    std::cerr << "Aufruf:\n"
              << "  baludesk_probe pair <server-url>   Paarung durchführen\n"
              << "  baludesk_probe status              abgelegte Sitzung prüfen\n"
              << "  baludesk_probe logout              Sitzung entfernen\n";
}

// Baut eine TLS-Regel. Bei unbekanntem Zertifikat wird der Fingerabdruck
// angezeigt und auf der Kommandozeile bestätigt - genau das, was in Stufe 1c
// ein Dialog tut.
bool establishTrust(HttpClient& http, std::string& pinOut) {
    HttpResult result = http.get("/api/health");

    if (result.error == HttpErrorKind::CertificatePinMismatch) {
        std::cerr << "Das Zertifikat weicht vom bestätigten ab. Abbruch.\n";
        return false;
    }

    if (result.error == HttpErrorKind::CertificateUntrusted) {
        if (result.peerCertificatePem.empty()) {
            std::cerr << "Zertifikat nicht vertrauenswürdig, und es war keines auszulesen.\n";
            return false;
        }
        const auto identity = identifyCertificate(result.peerCertificatePem);
        if (!identity) {
            std::cerr << "Zertifikat der Gegenstelle ist nicht lesbar.\n";
            return false;
        }

        std::cout << "\nDem Zertifikat dieses Servers wird nicht vertraut.\n"
                  << "Grund:         " << result.detail << "\n"
                  << "Fingerabdruck: " << identity->fingerprintSha256 << "\n\n"
                  << "Vergleiche ihn mit dem Server. Vertrauen? [j/N] ";

        std::string answer;
        std::getline(std::cin, answer);
        if (answer != "j" && answer != "J") {
            std::cerr << "Abgebrochen.\n";
            return false;
        }

        pinOut = identity->publicKeyPin;
        http.setTlsPolicy(TlsPolicy::pinnedFromConfirmedCertificate(pinOut));

        result = http.get("/api/health");
        if (!result.ok()) {
            std::cerr << "Auch mit angeheftetem Schlüssel keine Verbindung: "
                      << result.detail << "\n";
            return false;
        }
        std::cout << "Zertifikat bestätigt und angeheftet.\n";
        return true;
    }

    if (!result.ok()) {
        std::cerr << "Server nicht erreichbar: " << result.detail
                  << " (Status " << result.statusCode << ")\n";
        return false;
    }

    std::cout << "Zertifikat regulär geprüft, kein Anheften nötig.\n";
    return true;
}

int commandPair(const std::string& rawUrl) {
    const auto check = checkServerUrl(rawUrl);
    if (!check.ok()) {
        std::cerr << check.message << "\n";
        return 2;
    }
    std::cout << "Server: " << check.normalized << "\n";

    auto transport = std::make_shared<CurlTransport>();
    HttpClient http(check.normalized, transport, TlsPolicy{});

    std::string pin;
    if (!establishTrust(http, pin)) {
        return 1;
    }

    PairingClient pairing(http);
    auto progress = pairing.begin("baludesk-probe", "BaluDesk Pruefprogramm", nowSeconds());

    if (progress.state != PairingState::WaitingForApproval) {
        std::cerr << "Paarung nicht gestartet: " << progress.message << "\n";
        return 1;
    }

    std::cout << "\n=========================================\n"
              << "  Code:    " << pairing.prompt()->userCode << "\n"
              << "  Adresse: " << pairing.prompt()->verificationUrl << "\n"
              << "=========================================\n\n"
              << "Im Browser öffnen, anmelden, Code eingeben und freigeben.\n"
              << "Warte…\n";

    while (progress.state == PairingState::WaitingForApproval) {
        std::this_thread::sleep_for(std::chrono::seconds(progress.waitSeconds));
        progress = pairing.poll(nowSeconds());
        if (!progress.message.empty()) {
            std::cout << progress.message << "\n";
        }
    }

    if (progress.state != PairingState::Approved) {
        std::cerr << "Paarung nicht abgeschlossen.\n";
        return 1;
    }

    // Sofort ablegen. Der Server hat seinen Datensatz beim Ausliefern
    // gelöscht - was hier verloren geht, ist endgültig verloren.
    Session session;
    session.serverUrl = check.normalized;
    session.username = pairing.tokens()->username;
    session.publicKeyPin = pin;
    session.accessToken = pairing.tokens()->accessToken;
    session.refreshToken = pairing.tokens()->refreshToken;

    WindowsCredentialStore secrets;
    FileSessionRepository repository(sessionPath(), secrets);
    if (!repository.save(session)) {
        std::cerr << "Die Sitzung konnte nicht abgelegt werden. "
                     "Die Token sind damit verloren, bitte neu paaren.\n";
        return 1;
    }

    std::cout << "\nGepaart. Verbunden mit " << session.serverUrl
              << " als " << session.username << "\n";
    return 0;
}

int commandStatus() {
    WindowsCredentialStore secrets;
    FileSessionRepository repository(sessionPath(), secrets);

    const auto session = repository.load();
    if (!session) {
        std::cout << "Keine Sitzung abgelegt.\n";
        return 1;
    }

    std::cout << "Verbunden mit " << session->serverUrl << " als " << session->username << "\n"
              << "Zertifikat:   "
              << (session->publicKeyPin.empty() ? "regulär geprüft" : "angeheftet") << "\n";

    auto transport = std::make_shared<CurlTransport>();
    HttpClient http(session->serverUrl, transport,
                    TlsPolicy::pinnedFromConfirmedCertificate(session->publicKeyPin));

    const HttpResult result = http.get("/api/auth/me", session->accessToken);
    std::cout << "\n/api/auth/me → Status " << result.statusCode
              << (result.ok() ? " (gültig)" : " (nicht gültig)") << "\n";
    if (!result.body.empty()) {
        std::cout << result.body << "\n";
    }
    if (!result.detail.empty()) {
        std::cout << "Detail: " << result.detail << "\n";
    }

    return result.ok() ? 0 : 1;
}

int commandLogout() {
    WindowsCredentialStore secrets;
    FileSessionRepository repository(sessionPath(), secrets);
    repository.clear();
    std::cout << "Sitzung entfernt.\n";
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 2;
    }

    const std::string command = argv[1];

    if (command == "pair") {
        if (argc < 3) {
            printUsage();
            return 2;
        }
        return commandPair(argv[2]);
    }
    if (command == "status") {
        return commandStatus();
    }
    if (command == "logout") {
        return commandLogout();
    }

    printUsage();
    return 2;
}
```

- [ ] **Step 2: Bauen und den Aufruf ohne Argumente prüfen**

```powershell
cmake --preset msvc2022
cmake --build --preset debug
& "build\msvc2022\tools\probe\Debug\baludesk_probe.exe"
```

Erwartet: die drei Aufrufzeilen, Rückgabewert 2.

- [ ] **Step 3: Die Adressprüfung an der Kommandozeile bestätigen**

```powershell
& "build\msvc2022\tools\probe\Debug\baludesk_probe.exe" pair "http://nas.example.com"
```

Erwartet: die Meldung, dass unverschlüsseltes http nur zu localhost erlaubt ist, Rückgabewert 2. **Kein** Netzwerkverkehr — die Prüfung greift vorher.

- [ ] **Step 4: Gegen den echten Server paaren**

Dies ist die Abnahme der Stufe. Der Server muss laufen und ein Browser erreichbar sein.

```powershell
& "build\msvc2022\tools\probe\Debug\baludesk_probe.exe" pair "baluhost.duckdns.org"
```

Erwartet, in dieser Reihenfolge:
1. `Server: https://baluhost.duckdns.org` — das fehlende Schema wurde ergänzt
2. `Zertifikat regulär geprüft, kein Anheften nötig.`
3. Ein sechsstelliger Code und die Freigabeadresse
4. Nach der Freigabe im Browser: `Gepaart. Verbunden mit … als …`

Die vollständige Ausgabe gehört in den Bericht. Wird die Paarung im Browser abgelehnt, muss `Die Paarung wurde abgelehnt.` erscheinen und der Rückgabewert 1 sein — auch das ist ein gültiges Ergebnis dieses Schritts, dann anschließend erneut paaren.

- [ ] **Step 5: Den Neustart überleben — das eigentliche Abnahmekriterium**

```powershell
& "build\msvc2022\tools\probe\Debug\baludesk_probe.exe" status
```

Erwartet: `Verbunden mit … als …` und `/api/auth/me → Status 200 (gültig)` mit dem Benutzerdatensatz im Rumpf. Der Prozess ist ein anderer als beim Paaren — dass die Sitzung hier steht, ist genau der Nachweis, den Stufe 1 verlangt.

Zusätzlich prüfen, dass in der Ablagedatei kein Token steht:

```powershell
Get-Content "$env:LOCALAPPDATA\BaluHost\BaluDesk\session.json" -Encoding UTF8
```

Erwartet: `server_url`, `username` und `public_key_pin` — **keine** Token. Die stehen im Anmeldeinformationsspeicher:

```powershell
cmdkey /list | Select-String "BaluDesk:session"
```

- [ ] **Step 6: Den angehefteten Weg prüfen**

Derselbe Server über den Namen, für den das Zertifikat nicht ausgestellt ist — das erzwingt die Bestätigung.

```powershell
& "build\msvc2022\tools\probe\Debug\baludesk_probe.exe" logout
& "build\msvc2022\tools\probe\Debug\baludesk_probe.exe" pair "baluhost.local"
```

Erwartet: der Fingerabdruck wird angezeigt und die Bestätigung abgefragt. Nach `j` erscheint `Zertifikat bestätigt und angeheftet.`, danach läuft die Paarung wie zuvor. Anschließend:

```powershell
& "build\msvc2022\tools\probe\Debug\baludesk_probe.exe" status
```

Erwartet: `Zertifikat: angeheftet` und wieder Status 200 — der gespeicherte Pin trägt also über einen Prozesswechsel.

Prüfe außerdem, dass die Ablehnung wirkt: erneut `logout`, dann `pair "baluhost.local"` und bei der Frage `n` antworten. Erwartet: `Abgebrochen.`, Rückgabewert 1, **keine** abgelegte Sitzung.

- [ ] **Step 7: Aufräumen und committen**

Die Sitzung des Prüfprogramms stehen lassen oder entfernen, wie es passt — sie stört nicht. Dann:

```powershell
git add tools/probe
git commit -m "feat(tools): Pruefprogramm kann paaren, Sitzung zeigen und abmelden"
```

---

## Abnahme Stufe 1b

Aus einem gelöschten Build-Verzeichnis:

```powershell
cd "F:\Programme (x86)\BaluDesk QT"
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
cmake --preset msvc2022
cmake --build --preset debug
ctest --preset debug
```

Alle sechs Punkte müssen zutreffen:

1. Konfigurieren und Bauen ohne Fehler und ohne Warnungen aus eigenem Code
2. `ctest --preset debug` meldet **107 bestandene Tests**, 0 fehlgeschlagen
3. `baludesk_probe pair "http://nas.example.com"` wird abgelehnt, ohne das Netz anzufassen
4. Eine Paarung gegen `baluhost.duckdns.org` läuft durch und endet mit `Gepaart.`
5. `baludesk_probe status` im **neuen Prozess** meldet Status 200 — die Sitzung hat den Neustart überlebt
6. Über `baluhost.local` wird der Fingerabdruck abgefragt, nach Bestätigung gepaart, und `status` meldet `Zertifikat: angeheftet`

Punkt 5 ist das Abnahmekriterium der Stufe aus Spec Abschnitt 12.

---

## Was Stufe 1c braucht und hier bewusst fehlt

- **Drei Dialoge:** Server-URL eingeben (mit `checkServerUrl` als Prüfung), Zertifikat bestätigen (Fingerabdruck aus `identifyCertificate`), Code anzeigen samt `verification_url` und Fortschritt.
- **`poll` gehört an einen `QTimer`**, nicht in eine Schleife mit `sleep`. Der `PairingClient` gibt genau dafür `waitSeconds` zurück. Das Prüfprogramm schläft, weil es einen einzigen Ablauf fährt und nichts anderes zu tun hat — eine Oberfläche darf das nicht.
- **`HttpClient` blockiert bis zu 30 Sekunden** und lässt sich nicht abbrechen (Spec Abschnitt 2.10). Der Aufruf gehört deshalb nicht in den GUI-Thread. `setTlsPolicy` ist gegen `send` synchronisiert, der Bestätigungsdialog darf die Regel also aus dem GUI-Thread setzen.
- **Token erneuern:** `POST /api/auth/refresh` mit dem Erneuerungstoken. Es wird **nicht** rotiert — läuft es ab, hilft kein Erneuern, dann muss neu gepaart werden. Dieser Fall ist von einem gewöhnlichen 401 zu unterscheiden. In Stufe 1b bewusst ausgelassen, weil er ohne laufende Sitzung über Tage nicht sinnvoll zu prüfen ist.
- **Abmelden über die Oberfläche** ruft `SessionRepository::clear()` — die Logik steht, es fehlt der Knopf.
