# BaluDesk QT — Stufe 1c (Pairing-Oberfläche und Token-Erneuerung) Implementierungsplan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** BaluDesk paart sich über die eigene Oberfläche mit einem BaluHost-Server, stellt die Sitzung beim Start ohne Zutun wieder her, erneuert das Zugangstoken still und meldet sich auf Knopfdruck ab — belegt durch eine echte Paarung gegen den laufenden Server.

**Architecture:** Alles Prüfbare bleibt Qt-frei in `baludesk_core`: die Vertrauensentscheidung als Zustandsobjekt (`TrustNegotiation`) und die Erneuerung als Hülle um `HttpClient` (`AuthenticatedClient`). Dazwischen liegt `baludesk_bridge` mit genau einem `QObject`, das den gesamten blockierenden Teil besitzt und über Signale mit der Oberfläche redet; wo dieses Objekt lebt, entscheidet allein sein Eigentümer. Die Oberfläche ist ein `QStackedWidget` mit fünf Seiten und kennt weder `HttpClient` noch Threads.

**Tech Stack:** C++17 · Qt 6.8.0 Widgets · libcurl 8.21 mit OpenSSL · nlohmann/json · spdlog · GoogleTest

**Spec:** `docs/superpowers/specs/2026-07-27-stufe-1c-pairing-oberflaeche-design.md`
**Übergeordnete Spec:** `docs/superpowers/specs/2026-07-26-baludesk-qt-design.md` — Abschnitte 2.4, 2.6, 2.9, 2.10, 5, 12
**Übergabe aus 1b:** `docs/superpowers/uebergabe-an-stufe-1c.md`
**Ausgangspunkt:** Commit `9fc95f9`, **122 Tests grün**

### Zwei Verfeinerungen gegenüber der Spec

Beim Ausschreiben der Schnittstellen kamen zwei Stellen ans Licht, an denen die Spec zu knapp war. Beide sind Erweiterungen, keine Kursänderungen:

1. **`certificateChanged` trägt einen Grund mit.** Die Spec nennt `certificateChanged(QString serverUrl)`. Der Fall tritt aber aus zwei Richtungen ein — der angeheftete Pin passt nicht mehr (`CertificatePinMismatch`), oder ein Zertifikat ohne Pin ist nicht mehr prüfbar (`CertificateUntrusted`, etwa abgelaufen). Beide gehören auf dieselbe Seite, brauchen dort aber verschiedene Sätze. Signatur deshalb `certificateChanged(QString serverUrl, QString reason)`.
2. **`AuthBridge::pollNow()` ist ein öffentlicher Slot.** Sonst wäre der Abfrageschritt nur über einen laufenden `QTimer` erreichbar und die Tests müssten echte Sekunden verstreichen lassen.

---

## Global Constraints

- **C++17**, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`, `cmake_minimum_required(VERSION 3.25)`
- **`baludesk_core` darf niemals Qt linken.** Kein `QString`, kein `QObject`, kein Qt-Header unter `src/core/`. Die Absicherung steht in der Wurzel-`CMakeLists.txt` nach `add_subdirectory(tests)` und bricht mit `FATAL_ERROR` ab.
- **`src/bridge/` darf Qt kennen, aber keine Widgets.** `baludesk_bridge` linkt `Qt6::Core`, nicht `Qt6::Widgets`. Ein Dialog gehört nach `src/app/`.
- **Namensräume:** `baludesk::core` für `src/core/`, `baludesk::bridge` für `src/bridge/`, `baludesk::app` für `src/app/`
- **Include-Pfade immer vollständig ab `src/`** — `#include "core/auth/PairingClient.h"`, nie relativ mit `../`. In Tests ebenso ab `tests/`: `#include "support/FakeTransport.h"`.
- **Sprache:** Bezeichner und Dateinamen englisch, Kommentare und Protokolltexte deutsch
- **Umlaute:** echte UTF-8-Zeichen (`ü`, `ö`, `ä`, `ß`), Dateien als UTF-8 **ohne BOM**. Keine Behelfsschreibung. **Einzige Ausnahme: Git-Commit-Nachrichten bleiben ASCII**, weil PowerShell-5.1-Here-Strings Umlaute verstümmeln.
- **Generator:** `Visual Studio 17 2022`, x64. Warnungen `/W4 /permissive- /utf-8`, **keine Warnungen aus eigenem Code**.
- **Shell ist PowerShell 5.1:** kein `&&`. Mit `;` verketten, für „nur bei Erfolg" `if ($?) { … }`. Pfade mit Leerzeichen quoten.
- **Bauen und testen** immer über die Presets:
  ```powershell
  cmake --preset msvc2022
  cmake --build --preset debug
  ctest --preset debug
  ```
- **Nicht in Stufe 1c:** Synchronisation, Datenbank, Infobereichssymbol, Autostart, Benachrichtigungen, Dashboard, Explorer, Einstellungsdialog, Ändern des Gerätenamens, mehrere Server-Profile

---

## Dateistruktur nach Abschluss

```
src/core/
├── api/
│   ├── TrustNegotiation.h/.cpp     NEU  Vertrauensentscheidung ohne Ein-/Ausgabe
│   └── ServerUrl.h, TlsPolicy.h         unverändert
└── auth/
    ├── AuthenticatedClient.h/.cpp  NEU  401 -> erneuern -> genau einmal wiederholen
    ├── SessionRepository.h         GEÄNDERT  saveAccessToken(), clear() -> bool
    ├── FileSessionRepository.h/.cpp GEÄNDERT  dazu Fehlercodes auswerten
    └── SessionPaths.cpp            GEÄNDERT  Ausweichpfad bleibt absolut

src/bridge/                         NEU (ganzes Verzeichnis)
├── CMakeLists.txt
└── AuthBridge.h/.cpp                    das eine QObject mit dem blockierenden Teil

src/app/
├── MainWindow.h/.cpp               GEÄNDERT  QStackedWidget, QThread, Verdrahtung
├── SettingsStore.h/.cpp            GEÄNDERT  deviceId(), deviceName()
├── ConnectedPage.h/.cpp            NEU
└── pairing/                        NEU
    ├── Countdown.h/.cpp                 formatRemaining() - die einzige prüfbare Regel der Oberfläche
    ├── ServerUrlPage.h/.cpp
    ├── CertificatePage.h/.cpp
    ├── PairingCodePage.h/.cpp
    └── CertificateChangedPage.h/.cpp

tests/
├── core/trust_negotiation_test.cpp     NEU
├── core/authenticated_client_test.cpp  NEU
├── core/support/FakeSessionRepository.h NEU
├── core/support/FakeSecretStore.h      GEÄNDERT  failOnEraseNumber
├── core/session_repository_test.cpp    GEÄNDERT
├── core/session_paths_test.cpp         GEÄNDERT
├── app/settings_store_test.cpp         GEÄNDERT
├── app/countdown_test.cpp              NEU
└── bridge/auth_bridge_test.cpp         NEU

tools/probe/main.cpp                GEÄNDERT  fährt auf TrustNegotiation
```

**Warum `bridge/` eine eigene Bibliothek wird und nicht in `app/` liegt:** `baludesk_app_lib` linkt `Qt6::Widgets`. Läge `AuthBridge` darin, könnte jederzeit ein `QMessageBox` in den Arbeiterthread rutschen — Widgets sind außerhalb des GUI-Threads verboten, und der Fehler zeigt sich erst zur Laufzeit. Eine Bibliothek, die nur `Qt6::Core` linkt, macht daraus einen Übersetzungsfehler.

**Testzahlen je Task** (Ausgangspunkt 122):

| Task | neu | Summe |
|---|---|---|
| 1 TrustNegotiation | 12 | 134 |
| 2 Repository und Pfade | 8 | 142 |
| 3 AuthenticatedClient | 11 | 153 |
| 4 Gerätekennung | 4 | 157 |
| 5 AuthBridge: Sitzung und Abmelden | 7 | 164 |
| 6 AuthBridge: Paaren | 9 | 173 |
| 7 Seiten der Oberfläche | 3 | 176 |
| 8 MainWindow | 0 | 176 |
| 9 Abnahme | 0 | 176 |

---

## Task 1: TrustNegotiation — die Vertrauensentscheidung aus dem Prüfprogramm herausziehen

Der Ablauf „nicht vertrauenswürdig → Fingerabdruck zeigen → bestätigen → anheften → erneut prüfen" steckt heute in `tools/probe/main.cpp:52-104` und ist mit `std::cin`/`std::cout` verwachsen. Die Übergabe aus 1b benennt das als Falle: wer den Dialog schreibt, baut ihn sonst von Null nach, ohne Tests zu erben.

**Files:**
- Create: `src/core/api/TrustNegotiation.h`
- Create: `src/core/api/TrustNegotiation.cpp`
- Modify: `src/core/CMakeLists.txt`
- Test: `tests/core/trust_negotiation_test.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `tools/probe/main.cpp:49-104` (`establishTrust` ersetzen) und `tools/probe/main.cpp:117-120` (Aufrufstelle)

**Interfaces:**
- Consumes: `HttpClient::get`, `HttpClient::setTlsPolicy`, `HttpResult`, `HttpErrorKind`, `TlsPolicy::pinnedFromConfirmedCertificate`, `identifyCertificate` — alle aus Stufe 1a, unverändert
- Produces:
  - `enum class baludesk::core::TrustState { Idle, Trusted, ConfirmationRequired, PinMismatch, Unreachable, Failed }`
  - `struct baludesk::core::TrustPrompt { std::string fingerprintSha256; std::string publicKeyPin; std::string reason; }`
  - `class baludesk::core::TrustNegotiation` mit
    `explicit TrustNegotiation(HttpClient& http)`,
    `TrustState probe()`,
    `TrustState confirm()`,
    `const std::optional<TrustPrompt>& prompt() const`,
    `const std::string& confirmedPin() const`,
    `TrustState state() const`,
    `const std::string& message() const`

---

- [ ] **Step 1: Den fehlschlagenden Test schreiben**

Datei `tests/core/trust_negotiation_test.cpp`:

```cpp
#include "core/api/TrustNegotiation.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "core/api/HttpClient.h"
#include "support/FakeTransport.h"

using baludesk::core::HttpClient;
using baludesk::core::HttpErrorKind;
using baludesk::core::TlsPolicy;
using baludesk::core::TransportResult;
using baludesk::core::TrustNegotiation;
using baludesk::core::TrustState;
using baludesk::core::test::FakeTransport;

namespace {

TransportResult okResult() {
    TransportResult result;
    result.error = HttpErrorKind::None;
    result.response.statusCode = 200;
    result.response.body = R"({"status":"ok"})";
    return result;
}

TransportResult statusResult(long statusCode) {
    TransportResult result;
    result.error = HttpErrorKind::None;
    result.response.statusCode = statusCode;
    return result;
}

TransportResult failure(HttpErrorKind kind, std::string detail) {
    TransportResult result;
    result.error = kind;
    result.detail = std::move(detail);
    return result;
}

// Ein echtes Zertifikat, damit identifyCertificate() etwas zu tun bekommt -
// derselbe Weg wie in tls_trust_test.cpp. Der Pfad kommt beim Übersetzen
// herein, damit der Test unabhängig vom Arbeitsverzeichnis läuft.
std::string testCertificate() {
    const std::filesystem::path path =
        std::filesystem::path(BALUDESK_TEST_DATA_DIR) / "test-cert-a.pem";
    std::ifstream stream(path, std::ios::binary);
    EXPECT_TRUE(stream.good()) << "Testdatei nicht lesbar: " << path.string();
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

TransportResult untrustedResult(std::string pem) {
    TransportResult result;
    result.error = HttpErrorKind::CertificateUntrusted;
    result.detail = "self signed certificate";
    result.peerCertificatePem = std::move(pem);
    return result;
}

struct Fixture {
    std::shared_ptr<FakeTransport> transport = std::make_shared<FakeTransport>();
    std::unique_ptr<HttpClient> http;
    std::unique_ptr<TrustNegotiation> trust;

    Fixture() {
        http = std::make_unique<HttpClient>("https://nas.example.com", transport, TlsPolicy{});
        trust = std::make_unique<TrustNegotiation>(*http);
    }
};

}  // namespace

TEST(TrustNegotiationTest, StartsIdle) {
    Fixture f;
    EXPECT_EQ(f.trust->state(), TrustState::Idle);
    EXPECT_FALSE(f.trust->prompt().has_value());
    EXPECT_TRUE(f.trust->confirmedPin().empty());
}

TEST(TrustNegotiationTest, ReportsTrustedWhenTheCertificateChecksOut) {
    Fixture f;
    f.transport->queueResult(okResult());

    EXPECT_EQ(f.trust->probe(), TrustState::Trusted);
    EXPECT_FALSE(f.trust->prompt().has_value());
    // Kein Pin: bei regulärer Prüfung gibt es nichts anzuheften, und ein
    // trotzdem gesetzter Wert würde die Sitzung grundlos an ein Zertifikat
    // binden, das sich jederzeit erneuern darf.
    EXPECT_TRUE(f.trust->confirmedPin().empty());

    ASSERT_EQ(f.transport->calls.size(), std::size_t{1});
    EXPECT_EQ(f.transport->calls[0].url, "https://nas.example.com/api/health");
}

TEST(TrustNegotiationTest, AsksForConfirmationOnAnUnknownCertificate) {
    Fixture f;
    f.transport->queueResult(untrustedResult(testCertificate()));

    EXPECT_EQ(f.trust->probe(), TrustState::ConfirmationRequired);
    ASSERT_TRUE(f.trust->prompt().has_value());
    EXPECT_FALSE(f.trust->prompt()->fingerprintSha256.empty());
    EXPECT_FALSE(f.trust->prompt()->publicKeyPin.empty());
    EXPECT_EQ(f.trust->prompt()->reason, "self signed certificate");
    // Erst nach confirm() gilt der Pin als bestätigt.
    EXPECT_TRUE(f.trust->confirmedPin().empty());
}

TEST(TrustNegotiationTest, ReportsAPinMismatchWithoutAsking) {
    Fixture f;
    f.transport->queueResult(failure(HttpErrorKind::CertificatePinMismatch, "pinned key mismatch"));

    // Spec 2.9: bei einer Pin-Abweichung wird nicht nachgefragt.
    EXPECT_EQ(f.trust->probe(), TrustState::PinMismatch);
    EXPECT_FALSE(f.trust->prompt().has_value());
    EXPECT_FALSE(f.trust->message().empty());
}

TEST(TrustNegotiationTest, TreatsANetworkFailureAsUnreachable) {
    Fixture f;
    f.transport->queueResult(failure(HttpErrorKind::Network, "connection refused"));

    // Ein schlafender NAS ist laut Spec Abschnitt 10 Betriebszustand, nicht
    // Fehler - und ganz sicher keine Aussage über sein Zertifikat.
    EXPECT_EQ(f.trust->probe(), TrustState::Unreachable);
    EXPECT_FALSE(f.trust->message().empty());
}

TEST(TrustNegotiationTest, TreatsATimeoutAsUnreachable) {
    Fixture f;
    f.transport->queueResult(failure(HttpErrorKind::Timeout, "timed out"));

    EXPECT_EQ(f.trust->probe(), TrustState::Unreachable);
}

TEST(TrustNegotiationTest, FailsWhenNoCertificateCouldBeHarvested) {
    Fixture f;
    f.transport->queueResult(untrustedResult(""));

    // Ohne Zertifikat gibt es keinen Fingerabdruck zu zeigen. Eine Nachfrage
    // ohne Vergleichsmöglichkeit wäre eine Klickübung, kein Sicherheitsschritt.
    EXPECT_EQ(f.trust->probe(), TrustState::Failed);
    EXPECT_FALSE(f.trust->prompt().has_value());
    EXPECT_FALSE(f.trust->message().empty());
}

TEST(TrustNegotiationTest, FailsOnAnUnreadableCertificate) {
    Fixture f;
    f.transport->queueResult(untrustedResult("das ist kein PEM"));

    EXPECT_EQ(f.trust->probe(), TrustState::Failed);
    EXPECT_FALSE(f.trust->prompt().has_value());
}

TEST(TrustNegotiationTest, FailsWhenTheServerAnswersWithAnUnexpectedStatus) {
    Fixture f;
    // 404 auf /api/health: die Verbindung steht, aber am anderen Ende ist
    // kein BaluHost. Weiterzupaaren würde später an unverständlicher Stelle
    // scheitern.
    f.transport->queueResult(statusResult(404));

    EXPECT_EQ(f.trust->probe(), TrustState::Failed);
    EXPECT_FALSE(f.trust->message().empty());
}

TEST(TrustNegotiationTest, ConfirmPinsTheKeyAndChecksAgain) {
    Fixture f;
    f.transport->queueResult(untrustedResult(testCertificate()));
    ASSERT_EQ(f.trust->probe(), TrustState::ConfirmationRequired);
    const std::string offeredPin = f.trust->prompt()->publicKeyPin;

    f.transport->queueResult(okResult());
    EXPECT_EQ(f.trust->confirm(), TrustState::Trusted);

    EXPECT_EQ(f.trust->confirmedPin(), offeredPin);
    ASSERT_EQ(f.transport->calls.size(), std::size_t{2});
    // Der zweite Aufruf muss die angeheftete Regel tragen - sonst wäre die
    // Bestätigung folgenlos geblieben.
    EXPECT_EQ(f.transport->calls[1].policy.mode, TlsPolicy::Mode::Pinned);
    EXPECT_EQ(f.transport->calls[1].policy.publicKeyPin, offeredPin);
}

TEST(TrustNegotiationTest, ConfirmWithoutAPromptTouchesNothing) {
    Fixture f;

    // Nichts zu bestätigen. Der Aufruf darf weder ins Netz greifen noch
    // irgendetwas anheften.
    EXPECT_EQ(f.trust->confirm(), TrustState::Idle);
    EXPECT_TRUE(f.transport->calls.empty());
    EXPECT_TRUE(f.trust->confirmedPin().empty());
}

TEST(TrustNegotiationTest, ConfirmKeepsNoPinWhenTheCheckStillFails) {
    Fixture f;
    f.transport->queueResult(untrustedResult(testCertificate()));
    ASSERT_EQ(f.trust->probe(), TrustState::ConfirmationRequired);

    f.transport->queueResult(failure(HttpErrorKind::Network, "connection reset"));
    EXPECT_EQ(f.trust->confirm(), TrustState::Unreachable);

    // Ein Pin, der nie zu einer stehenden Verbindung geführt hat, gehört
    // nicht in die Sitzung - er würde dort dauerhaft eine Abweichung
    // erzeugen, die niemand mehr zuordnen kann.
    EXPECT_TRUE(f.trust->confirmedPin().empty());
}
```

- [ ] **Step 2: `tests/CMakeLists.txt` erweitern**

In der Quelldateiliste von `baludesk_core_tests` eine Zeile ergänzen, alles Übrige unverändert lassen:

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
    core/session_paths_test.cpp
    core/trust_negotiation_test.cpp
)
```

`BALUDESK_TEST_DATA_DIR` und `target_include_directories(... ${CMAKE_CURRENT_SOURCE_DIR})` stehen dort bereits — der neue Test braucht beides und bekommt es ohne weiteres Zutun.

- [ ] **Step 3: Test laufen lassen und Fehlschlag bestätigen**

```powershell
cmake --preset msvc2022
cmake --build --preset debug
```

Erwartet: FEHLER beim Übersetzen — `Cannot open include file: 'core/api/TrustNegotiation.h'`.

- [ ] **Step 4: `src/core/api/TrustNegotiation.h` schreiben**

```cpp
#pragma once

#include <optional>
#include <string>

namespace baludesk::core {

class HttpClient;

// Wo die Vertrauensentscheidung über ein Serverzertifikat steht.
enum class TrustState {
    // Noch nichts geprüft.
    Idle,
    // Regulär geprüft oder der angeheftete Schlüssel passt. Verbindung steht.
    Trusted,
    // Unbekanntes Zertifikat. prompt() ist belegt, ein Mensch muss den
    // Fingerabdruck vergleichen.
    ConfirmationRequired,
    // Der angeheftete Schlüssel passt nicht mehr. Spec Abschnitt 2.9: harter
    // Abbruch, keine Nachfrage.
    PinMismatch,
    // Netz weg, Zeitlimit. Sagt nichts über das Zertifikat aus.
    Unreachable,
    // Zertifikat unlesbar oder gar nicht auszulesen, unerwartete Antwort.
    Failed
};

// Was dem Nutzer zu zeigen ist, damit er entscheiden kann.
struct TrustPrompt {
    // Zum Anzeigen und Vergleichen, in der Darstellung, die auch Browser und
    // die openssl-Kommandozeile ausgeben.
    std::string fingerprintSha256;

    // Zum Anheften, im Format, das libcurl erwartet.
    std::string publicKeyPin;

    // Warum die reguläre Prüfung fehlschlug, im Klartext der Gegenstelle.
    std::string reason;
};

// Der Ablauf „prüfen - bei Bedarf bestätigen lassen - anheften - erneut
// prüfen" als Zustandsobjekt ohne jede Ein- und Ausgabe.
//
// Die Klasse fragt niemanden und schreibt nichts: sie sagt, was zu tun ist,
// und wartet auf die Antwort. Genau deshalb kann derselbe Ablauf einmal von
// einem Dialog und einmal vom Prüfprogramm gefahren werden, ohne dass einer
// von beiden ihn nachbaut - und deshalb ist er ohne Netz prüfbar.
//
// Die Klasse setzt die TLS-Regel des übergebenen HttpClient. Sie besitzt ihn
// nicht; er muss sie überleben.
class TrustNegotiation {
public:
    explicit TrustNegotiation(HttpClient& http);

    // Nimmt mit der aktuell gesetzten Regel Kontakt auf.
    TrustState probe();

    // Heftet den angebotenen Schlüssel an und prüft erneut. Nur nach
    // ConfirmationRequired wirksam; sonst bleibt alles, wie es war, und es
    // wird nichts gesendet.
    //
    // Der Pin gilt erst dann als bestätigt, wenn die erneute Prüfung
    // Trusted ergibt - ein Pin, der nie zu einer stehenden Verbindung
    // geführt hat, gehört nicht in die Sitzung.
    TrustState confirm();

    [[nodiscard]] const std::optional<TrustPrompt>& prompt() const { return prompt_; }

    // Leer, solange nichts bestätigt wurde - und ebenso bei regulärer
    // Prüfung, wo es nichts anzuheften gibt. Genau dieser Wert gehört in
    // Session::publicKeyPin.
    [[nodiscard]] const std::string& confirmedPin() const { return confirmedPin_; }

    [[nodiscard]] TrustState state() const { return state_; }

    // Erklärung im Klartext, zur Anzeige geeignet. Leer, wenn alles stimmt.
    [[nodiscard]] const std::string& message() const { return message_; }

private:
    // Ein Kontaktversuch samt Auswertung. Setzt state_, prompt_ und message_.
    TrustState evaluate();

    TrustState settle(TrustState state, std::string message);

    HttpClient& http_;

    TrustState state_ = TrustState::Idle;
    std::optional<TrustPrompt> prompt_;
    std::string confirmedPin_;
    std::string message_;
};

}  // namespace baludesk::core
```

- [ ] **Step 5: `src/core/api/TrustNegotiation.cpp` schreiben**

```cpp
#include "core/api/TrustNegotiation.h"

#include <utility>

#include "core/api/HttpClient.h"
#include "core/api/TlsPolicy.h"
#include "core/util/Logger.h"
#include "core/util/TlsTrust.h"

namespace baludesk::core {
namespace {

// Der günstigste Endpunkt für einen Kontaktversuch: ohne Anmeldung, ohne
// Nebenwirkung, und er existiert auf jedem BaluHost (Spec Abschnitt 9).
constexpr const char* kHealthPath = "/api/health";

}  // namespace

TrustNegotiation::TrustNegotiation(HttpClient& http) : http_(http) {}

TrustState TrustNegotiation::settle(TrustState state, std::string message) {
    state_ = state;
    message_ = std::move(message);
    return state_;
}

TrustState TrustNegotiation::evaluate() {
    const HttpResult result = http_.get(kHealthPath);

    if (result.error == HttpErrorKind::CertificatePinMismatch) {
        Logger::error("Vertrauen: Zertifikat weicht vom angehefteten Schlüssel ab");
        prompt_.reset();
        return settle(TrustState::PinMismatch,
                      "Der Server weist ein anderes Zertifikat vor als das beim Verbinden "
                      "bestätigte.");
    }

    if (result.error == HttpErrorKind::CertificateUntrusted) {
        if (result.peerCertificatePem.empty()) {
            Logger::warn("Vertrauen: Zertifikat nicht vertrauenswürdig und nicht auszulesen");
            prompt_.reset();
            return settle(TrustState::Failed,
                          "Dem Zertifikat des Servers wird nicht vertraut, und es war keines "
                          "auszulesen, das sich vergleichen ließe.");
        }

        const auto identity = identifyCertificate(result.peerCertificatePem);
        if (!identity) {
            Logger::warn("Vertrauen: Zertifikat der Gegenstelle ist nicht lesbar");
            prompt_.reset();
            return settle(TrustState::Failed,
                          "Das Zertifikat des Servers ist nicht lesbar.");
        }

        TrustPrompt readyPrompt;
        readyPrompt.fingerprintSha256 = identity->fingerprintSha256;
        readyPrompt.publicKeyPin = identity->publicKeyPin;
        readyPrompt.reason = result.detail;
        prompt_ = readyPrompt;

        Logger::info("Vertrauen: unbekanntes Zertifikat, Bestätigung nötig — {}", result.detail);
        return settle(TrustState::ConfirmationRequired, {});
    }

    if (result.error == HttpErrorKind::Network || result.error == HttpErrorKind::Timeout) {
        Logger::debug("Vertrauen: Server nicht erreichbar — {}", result.detail);
        return settle(TrustState::Unreachable,
                      "Der Server ist nicht erreichbar: " + result.detail);
    }

    if (result.error != HttpErrorKind::None) {
        Logger::warn("Vertrauen: Verbindung unbrauchbar — {}", result.detail);
        return settle(TrustState::Failed,
                      "Die Verbindung zum Server ist unbrauchbar: " + result.detail);
    }

    if (!result.ok()) {
        // Die Verbindung steht, aber am anderen Ende antwortet kein BaluHost.
        Logger::warn("Vertrauen: unerwarteter Status {} auf {}", result.statusCode, kHealthPath);
        return settle(TrustState::Failed,
                      "Unter dieser Adresse antwortet kein BaluHost-Server (Status " +
                          std::to_string(result.statusCode) + ").");
    }

    prompt_.reset();
    return settle(TrustState::Trusted, {});
}

TrustState TrustNegotiation::probe() {
    return evaluate();
}

TrustState TrustNegotiation::confirm() {
    if (state_ != TrustState::ConfirmationRequired || !prompt_) {
        // Nichts zu bestätigen. Bewusst ohne Netzzugriff und ohne
        // Zustandswechsel - dieselbe Bauart wie PairingClient::poll() nach
        // einem Endzustand.
        return state_;
    }

    const std::string offeredPin = prompt_->publicKeyPin;
    http_.setTlsPolicy(TlsPolicy::pinnedFromConfirmedCertificate(offeredPin));

    const TrustState result = evaluate();

    if (result == TrustState::Trusted) {
        confirmedPin_ = offeredPin;
        Logger::info("Vertrauen: Zertifikat bestätigt und Schlüssel angeheftet");
        return result;
    }

    if (result == TrustState::ConfirmationRequired) {
        // Kann baulich nicht eintreten: mit angehefteter Regel meldet der
        // Transport eine Abweichung als CertificatePinMismatch, nie als
        // CertificateUntrusted (belegt in der Abnahme der Stufe 1a). Der
        // Zweig steht hier, damit ein künftiger Umbau des Transports nicht
        // still in einer Endlosschleife aus Nachfragen landet.
        prompt_.reset();
        return settle(TrustState::Failed,
                      "Das Zertifikat ließ sich auch nach der Bestätigung nicht prüfen.");
    }

    // Nicht bestätigt: der Pin bleibt ungesetzt.
    confirmedPin_.clear();
    return result;
}

}  // namespace baludesk::core
```

- [ ] **Step 6: `src/core/CMakeLists.txt` erweitern**

`api/TrustNegotiation.cpp` in die Quelldateiliste aufnehmen. Alles Übrige — die `find_package`-Aufrufe, die `target_link_libraries`-Blöcke samt ihrer Kommentare, der `Advapi32`-Block — unverändert lassen:

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
    api/TrustNegotiation.cpp
    auth/PairingClient.cpp
    auth/FileSessionRepository.cpp
    auth/SessionPaths.cpp
)
```

- [ ] **Step 7: Tests laufen lassen und grün bestätigen**

```powershell
cmake --preset msvc2022
cmake --build --preset debug
ctest --preset debug
```

Erwartet: **134 Tests, alle bestanden** (122 vorhandene plus 12 neue).

- [ ] **Step 8: `tools/probe/main.cpp` auf die neue Klasse umstellen**

Damit ist die Extraktion belegt statt behauptet — derselbe Ablauf, gefahren von zwei Aufrufern.

Den Include-Block um `#include "core/api/TrustNegotiation.h"` ergänzen (alphabetisch hinter `core/api/TlsPolicy.h`).

Die gesamte Funktion `establishTrust` (`tools/probe/main.cpp:49-104`) durch diese ersetzen:

```cpp
// Baut eine TLS-Regel. Der Ablauf selbst liegt in core/api/TrustNegotiation -
// hier bleibt nur, was Kommandozeile ist: fragen und ausgeben. Genau diese
// Trennung erlaubt es der Oberfläche in Stufe 1c, denselben Ablauf mit einem
// Dialog zu fahren, statt ihn nachzubauen.
bool establishTrust(HttpClient& http, std::string& pinOut) {
    TrustNegotiation trust(http);

    TrustState state = trust.probe();

    if (state == TrustState::ConfirmationRequired) {
        std::cout << "\nDem Zertifikat dieses Servers wird nicht vertraut.\n"
                  << "Grund:         " << trust.prompt()->reason << "\n"
                  << "Fingerabdruck: " << trust.prompt()->fingerprintSha256 << "\n\n"
                  << "Vergleiche ihn mit dem Server. Vertrauen? [j/N] ";

        std::string answer;
        std::getline(std::cin, answer);
        if (answer != "j" && answer != "J") {
            std::cerr << "Abgebrochen.\n";
            return false;
        }

        state = trust.confirm();
    }

    if (state != TrustState::Trusted) {
        std::cerr << trust.message() << "\n";
        return false;
    }

    pinOut = trust.confirmedPin();
    if (pinOut.empty()) {
        std::cout << "Zertifikat regulär geprüft, kein Anheften nötig.\n";
    } else {
        std::cout << "Zertifikat bestätigt und angeheftet.\n";
    }
    return true;
}
```

- [ ] **Step 9: Bauen und den ganzen Durchlauf gegen den echten Server prüfen**

```powershell
cmake --build --preset debug
ctest --preset debug
```

Erwartet: 134 Tests grün, keine Warnungen aus eigenem Code.

Dann von Hand, weil nur das die Umstellung belegt (Ausgabe genau lesen, nicht nur den Rückgabewert):

```powershell
.\build\msvc2022\Debug\baludesk_probe.exe status
```

Erwartet: unveränderte Ausgabe wie vor der Umstellung — verbunden, `/api/auth/me → Status 200`.

- [ ] **Step 10: Committen**

```powershell
git add src/core/api/TrustNegotiation.h src/core/api/TrustNegotiation.cpp src/core/CMakeLists.txt tests/core/trust_negotiation_test.cpp tests/CMakeLists.txt tools/probe/main.cpp
git commit -m @'
feat(core): TrustNegotiation traegt die Vertrauensentscheidung

Der Ablauf steckte in tools/probe/main.cpp und war mit cin/cout verwachsen -
die Oberflaeche haette ihn von Null nachgebaut, ohne Tests zu erben. Jetzt ein
Zustandsobjekt ohne Ein- und Ausgabe, gefahren von zwei Aufrufern.

Ein Pin gilt erst als bestaetigt, wenn die erneute Pruefung steht.
'@
```

---

## Task 2: Sitzungsablage härten — Erneuerungspfad, Rückmeldung beim Abmelden, absoluter Pfad

Drei offene Punkte der Übergabe liegen in genau den Dateien, die Stufe 1c ohnehin anfasst. Sie jetzt mitzunehmen ist billiger als später, und `saveAccessToken()` wird in Task 3 gebraucht.

**Files:**
- Modify: `src/core/auth/SessionRepository.h`
- Modify: `src/core/auth/FileSessionRepository.h`
- Modify: `src/core/auth/FileSessionRepository.cpp`
- Modify: `src/core/auth/SessionPaths.cpp`
- Modify: `tests/core/support/FakeSecretStore.h`
- Test: `tests/core/session_repository_test.cpp` (ergänzen)
- Test: `tests/core/session_paths_test.cpp` (ergänzen)
- Modify: `tools/probe/main.cpp:203-209` (`commandLogout`)

**Interfaces:**
- Consumes: `SecretStore`, `Session`, `Logger` — unverändert
- Produces:
  - `virtual bool SessionRepository::saveAccessToken(const std::string& accessToken) = 0`
  - `virtual bool SessionRepository::clear() = 0` — **Signaturänderung**, vorher `void`
  - `FakeSecretStore::failOnEraseNumber`, `FakeSecretStore::eraseCallCount`

---

- [ ] **Step 1: Die fehlschlagenden Tests schreiben**

An `tests/core/session_repository_test.cpp` **anhängen** (die vorhandenen Tests unverändert lassen; `makeEmptyTempDir` und `exampleSession` stehen bereits oben in der Datei):

```cpp
TEST(SessionRepositoryTest, SaveAccessTokenReplacesOnlyTheAccessToken) {
    const auto directory = makeEmptyTempDir("token_ersetzen");
    FakeSecretStore secrets;
    FileSessionRepository repository((directory / "session.json").string(), secrets);
    ASSERT_TRUE(repository.save(exampleSession()));

    EXPECT_TRUE(repository.saveAccessToken("frisches-zugangstoken"));

    const auto loaded = repository.load();
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->accessToken, "frisches-zugangstoken");
    // Das Erneuerungstoken wird vom Server nicht rotiert - es darf hier auch
    // nicht verlorengehen.
    EXPECT_EQ(loaded->refreshToken, "erneuerungstoken");
    EXPECT_EQ(loaded->username, "sven");
    EXPECT_EQ(loaded->publicKeyPin, "sha256//AAAABBBBCCCC=");
}

TEST(SessionRepositoryTest, SaveAccessTokenLeavesTheFileUntouched) {
    const auto directory = makeEmptyTempDir("datei_unberuehrt");
    FakeSecretStore secrets;
    const std::string path = (directory / "session.json").string();
    FileSessionRepository repository(path, secrets);
    ASSERT_TRUE(repository.save(exampleSession()));

    const auto readFile = [&path]() {
        std::ifstream stream(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(stream),
                           std::istreambuf_iterator<char>());
    };
    const std::string before = readFile();

    ASSERT_TRUE(repository.saveAccessToken("frisches-zugangstoken"));

    // Der Grund für den eigenen Weg: beim Erneuern wechselt nur das
    // Geheimnis. Fasst dieser Pfad die Datei nicht an, kennt er auch das
    // Zeitfenster nicht, in dem neue Token neben alten Metadaten stehen.
    EXPECT_EQ(readFile(), before);
}

TEST(SessionRepositoryTest, SaveAccessTokenRefusesAnEmptyToken) {
    const auto directory = makeEmptyTempDir("token_leer");
    FakeSecretStore secrets;
    FileSessionRepository repository((directory / "session.json").string(), secrets);
    ASSERT_TRUE(repository.save(exampleSession()));

    // Ein leeres Zugangstoken abzulegen hiesse, eine funktionierende Sitzung
    // gegen eine kaputte zu tauschen.
    EXPECT_FALSE(repository.saveAccessToken(""));
    EXPECT_EQ(repository.load()->accessToken, "zugangstoken");
}

TEST(SessionRepositoryTest, SaveAccessTokenReportsAFailure) {
    const auto directory = makeEmptyTempDir("token_fehler");
    FakeSecretStore secrets;
    FileSessionRepository repository((directory / "session.json").string(), secrets);
    ASSERT_TRUE(repository.save(exampleSession()));

    // save() der Sitzung hat bereits zweimal abgelegt; der dritte Aufruf ist
    // der erste von saveAccessToken().
    secrets.failOnSaveNumber = 3;
    EXPECT_FALSE(repository.saveAccessToken("frisches-zugangstoken"));
}

TEST(SessionRepositoryTest, ClearReportsSuccessOnACompleteRemoval) {
    const auto directory = makeEmptyTempDir("abmelden_vollstaendig");
    FakeSecretStore secrets;
    const std::string path = (directory / "session.json").string();
    FileSessionRepository repository(path, secrets);
    ASSERT_TRUE(repository.save(exampleSession()));

    EXPECT_TRUE(repository.clear());
    EXPECT_FALSE(repository.load().has_value());
    EXPECT_FALSE(fs::exists(path));
    EXPECT_TRUE(secrets.entries.empty());
}

TEST(SessionRepositoryTest, ClearReportsFailureWhenASecretRemained) {
    const auto directory = makeEmptyTempDir("abmelden_rest");
    FakeSecretStore secrets;
    FileSessionRepository repository((directory / "session.json").string(), secrets);
    ASSERT_TRUE(repository.save(exampleSession()));

    // Der Abmelde-Knopf soll nicht „erledigt" melden, während ein Token im
    // Anmeldeinformationsspeicher zurückbleibt.
    secrets.failOnEraseNumber = 1;
    EXPECT_FALSE(repository.clear());
}

TEST(SessionRepositoryTest, ClearOnAnEmptyStoreReportsSuccess) {
    const auto directory = makeEmptyTempDir("abmelden_leer");
    FakeSecretStore secrets;
    FileSessionRepository repository((directory / "session.json").string(), secrets);

    // Nichts zu entfernen heisst: das Ziel ist erreicht.
    EXPECT_TRUE(repository.clear());
}
```

An `tests/core/session_paths_test.cpp` anhängen — dazu oben in der Datei `#include <cstdlib>` ergänzen:

```cpp
TEST(SessionPathsTest, StaysAbsoluteWhenLocalAppDataIsMissing) {
    // Der Header sagt „immer absolut" zu. Fehlt LOCALAPPDATA - in einem
    // Dienstkonto oder einer abgespeckten Umgebung durchaus möglich -, darf
    // das Ergebnis nicht am Arbeitsverzeichnis hängen: Oberfläche und
    // Prüfprogramm fänden sonst je nach Startort verschiedene Sitzungen.
#pragma warning(push)
#pragma warning(disable : 4996)
    const char* previous = std::getenv("LOCALAPPDATA");
#pragma warning(pop)
    const std::string saved = previous != nullptr ? previous : "";

    // Ein leerer Wert entfernt die Variable unter Windows.
    ASSERT_EQ(_putenv_s("LOCALAPPDATA", ""), 0);
    const std::string path = defaultSessionPath();
    if (!saved.empty()) {
        ASSERT_EQ(_putenv_s("LOCALAPPDATA", saved.c_str()), 0);
    }

    EXPECT_FALSE(path.empty());
    EXPECT_TRUE(std::filesystem::path(path).is_absolute());
}
```

- [ ] **Step 2: `FakeSecretStore` um den Fehlschalter für `erase` erweitern**

In `tests/core/support/FakeSecretStore.h` die Methode `erase` ersetzen und zwei Felder ergänzen. Der Rest der Datei bleibt unverändert:

```cpp
    bool erase(const std::string& key) override {
        ++eraseCallCount;
        if (eraseCallCount == failOnEraseNumber) {
            return false;
        }
        entries.erase(key);
        return true;
    }
```

Bei den vorhandenen Feldern am Ende der Klasse ergänzen:

```cpp
    // Lässt den n-ten erase()-Aufruf scheitern (1 = der erste). 0 heisst: nie.
    // Gegenstück zu failOnSaveNumber, aus demselben Grund als Zähler statt
    // als Schlüsselname: die Schlüssel sind private Konstanten von
    // FileSessionRepository.
    std::size_t failOnEraseNumber = 0;

    std::size_t eraseCallCount = 0;
```

- [ ] **Step 3: Tests laufen lassen und Fehlschlag bestätigen**

```powershell
cmake --build --preset debug
```

Erwartet: FEHLER beim Übersetzen — `'saveAccessToken': is not a member of 'baludesk::core::FileSessionRepository'`, dazu in `ClearReportsSuccessOnACompleteRemoval` ein Fehler, weil sich `void` nicht in `bool` umwandeln lässt.

- [ ] **Step 4: `src/core/auth/SessionRepository.h` erweitern**

Oben `#include <string>` ergänzen. Dann die Deklaration von `clear()` ersetzen und `saveAccessToken()` davor einfügen:

```cpp
    // Aktualisiert allein das Zugangstoken einer bereits abgelegten Sitzung.
    //
    // Getrennt von save(), weil beim Erneuern ausschliesslich dieser eine Wert
    // wechselt: Adresse, Benutzername und Pin - der gesamte Inhalt der Datei -
    // bleiben unberührt. Der Erneuerungspfad fasst die Datei damit gar nicht
    // an und kennt das Zeitfenster nicht, in dem neue Geheimnisse neben alten
    // Metadaten stehen könnten. Genau dieser Pfad wird ab Stufe 2 ständig
    // gefahren.
    //
    // Ein leeres Token wird abgelehnt: es würde eine funktionierende Sitzung
    // gegen eine kaputte tauschen.
    virtual bool saveAccessToken(const std::string& accessToken) = 0;

    // Entfernt alle Bestandteile. Auf einer leeren Ablage unschädlich.
    //
    // false, wenn etwas zurückblieb. Der Abmelde-Knopf soll nicht „erledigt"
    // melden, während ein Token im Anmeldeinformationsspeicher steht.
    virtual bool clear() = 0;
```

- [ ] **Step 5: `src/core/auth/FileSessionRepository.h` nachziehen**

Die beiden Deklarationen im `public`-Abschnitt ersetzen:

```cpp
    [[nodiscard]] std::optional<Session> load() override;
    bool save(const Session& session) override;
    bool saveAccessToken(const std::string& accessToken) override;
    bool clear() override;
```

- [ ] **Step 6: `src/core/auth/FileSessionRepository.cpp` anpassen**

**a)** In `save()` den Fehlercode von `create_directories` auswerten. Den Block bei `FileSessionRepository.cpp:126-130` ersetzen:

```cpp
    std::error_code error;
    const std::filesystem::path path(jsonPath_);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            // Ohne diese Prüfung wäre ein Verzeichnisfehler im Protokoll nicht
            // von einem Dateifehler zu unterscheiden - und der Rückbau der
            // Geheimnisse unterbliebe.
            Logger::error("Sitzung: Ablageverzeichnis konnte nicht angelegt werden — {}",
                          error.message());
            rollbackSecrets();
            return false;
        }
    }
```

**b)** `saveAccessToken()` neu einfügen, hinter `save()` und vor `clear()`:

```cpp
bool FileSessionRepository::saveAccessToken(const std::string& accessToken) {
    if (accessToken.empty()) {
        Logger::error("Sitzung: leeres Zugangstoken wird nicht abgelegt");
        return false;
    }

    // Bewusst nur das Geheimnis: die Datei trägt Adresse, Benutzername und
    // Pin, und keiner dieser Werte ändert sich beim Erneuern.
    if (!secrets_.save(kAccessTokenKey, accessToken)) {
        Logger::error("Sitzung: erneuertes Zugangstoken konnte nicht abgelegt werden");
        return false;
    }

    Logger::debug("Sitzung: Zugangstoken erneuert abgelegt");
    return true;
}
```

**c)** `clear()` (`FileSessionRepository.cpp:204-212`) vollständig ersetzen:

```cpp
bool FileSessionRepository::clear() {
    bool complete = true;

    if (!secrets_.erase(kAccessTokenKey)) {
        Logger::error("Sitzung: Zugangstoken konnte nicht entfernt werden");
        complete = false;
    }
    if (!secrets_.erase(kRefreshTokenKey)) {
        Logger::error("Sitzung: Erneuerungstoken konnte nicht entfernt werden");
        complete = false;
    }

    std::error_code error;
    // Der Rückgabewert von remove() unterscheidet „entfernt" von „war nicht
    // da"; beides ist Erfolg. Nur der Fehlercode zeigt an, dass es nicht ging.
    std::filesystem::remove(jsonPath_, error);
    if (error) {
        Logger::error("Sitzung: Ablagedatei konnte nicht entfernt werden — {}", error.message());
        complete = false;
    }

    if (complete) {
        Logger::info("Sitzung: entfernt");
    } else {
        Logger::warn("Sitzung: unvollständig entfernt, es blieb etwas zurück");
    }
    return complete;
}
```

- [ ] **Step 7: `src/core/auth/SessionPaths.cpp` auf einen absoluten Ausweichpfad bringen**

Die Datei vollständig ersetzen:

```cpp
#include "core/auth/SessionPaths.h"

#include <cstdlib>
#include <filesystem>
#include <system_error>

namespace baludesk::core {

std::string defaultSessionPath() {
    // std::getenv gilt bei MSVC unter /W4 als „unsicher" (C4996) und würde die
    // Warnungsfreiheit des Builds verletzen. Hier bewusst und begrenzt
    // unterdrückt statt projektweit, da nur diese eine Stelle betroffen ist.
#pragma warning(push)
#pragma warning(disable : 4996)
    const char* localAppData = std::getenv("LOCALAPPDATA");
#pragma warning(pop)

    std::filesystem::path base;
    if (localAppData != nullptr && *localAppData != '\0') {
        base = localAppData;
    }

    // Der Header sagt „immer absolut" zu. Das frühere Ausweichen auf "."
    // brach diese Zusage still: das Ergebnis hing dann am Arbeitsverzeichnis,
    // und Oberfläche und Prüfprogramm fänden je nach Startort verschiedene
    // Sitzungen - der Fehlermodus wäre „nicht gepaart" ohne jede Meldung.
    if (base.empty() || !base.is_absolute()) {
        std::error_code error;
        base = std::filesystem::temp_directory_path(error);
        if (error || base.empty()) {
            base = std::filesystem::current_path(error);
        }
    }

    return (base / "BaluHost" / "BaluDesk" / "session.json").string();
}

}  // namespace baludesk::core
```

- [ ] **Step 8: `tools/probe/main.cpp` an die neue Signatur anpassen**

`commandLogout` (`tools/probe/main.cpp:203-209`) ersetzen:

```cpp
int commandLogout() {
    WindowsCredentialStore secrets;
    FileSessionRepository repository(sessionPath(), secrets);

    if (!repository.clear()) {
        std::cerr << "Sitzung nur unvollständig entfernt. Im "
                     "Anmeldeinformationsspeicher unter „BaluDesk“ nachsehen.\n";
        return 1;
    }

    std::cout << "Sitzung entfernt.\n";
    return 0;
}
```

- [ ] **Step 9: Tests laufen lassen und grün bestätigen**

```powershell
cmake --build --preset debug
ctest --preset debug
```

Erwartet: **142 Tests, alle bestanden** (134 plus 8 neue). Keine Warnungen aus eigenem Code.

- [ ] **Step 10: Committen**

```powershell
git add src/core/auth tests/core/session_repository_test.cpp tests/core/session_paths_test.cpp tests/core/support/FakeSecretStore.h tools/probe/main.cpp
git commit -m @'
feat(core): eigener Weg fuers Erneuern, Rueckmeldung beim Abmelden

saveAccessToken() legt nur das Geheimnis ab - beim Erneuern aendert sich
sonst nichts, also faellt fuer den kuenftig staendig gefahrenen Pfad das
Zeitfenster zwischen Geheimnis und Datei ganz weg.

clear() liefert bool, damit der Abmelde-Knopf nicht "erledigt" meldet,
waehrend ein Token zurueckblieb. Dazu: Fehlercode von create_directories
ausgewertet und defaultSessionPath() haelt seine Zusage "immer absolut"
auch ohne LOCALAPPDATA.
'@
```

---

## Task 3: AuthenticatedClient — Erneuerung mit genau einem Versuch

**Files:**
- Create: `src/core/auth/AuthenticatedClient.h`
- Create: `src/core/auth/AuthenticatedClient.cpp`
- Create: `tests/core/support/FakeSessionRepository.h`
- Modify: `src/core/CMakeLists.txt`
- Test: `tests/core/authenticated_client_test.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `HttpClient::get`, `HttpClient::postJson`, `HttpResult`, `HttpErrorKind`, `Session`, sowie aus Task 2 `SessionRepository::saveAccessToken`
- Produces:
  - `enum class baludesk::core::AuthOutcome { Ok, Refreshed, ReauthRequired, TrustBroken, Unreachable }`
  - `struct baludesk::core::AuthenticatedResult { HttpResult response; AuthOutcome outcome; bool usable() const; }`
  - `class baludesk::core::AuthenticatedClient` mit
    `AuthenticatedClient(HttpClient& http, SessionRepository& repository, Session session)`,
    `AuthenticatedResult get(const std::string& path)`,
    `AuthenticatedResult postJson(const std::string& path, const std::string& jsonBody)`,
    `const Session& session() const`
  - `class baludesk::core::test::FakeSessionRepository` mit den Feldern
    `stored`, `saveCallCount`, `saveAccessTokenCallCount`, `failSaveAccessToken`, `failClear`

---

- [ ] **Step 1: `tests/core/support/FakeSessionRepository.h` schreiben**

```cpp
#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "core/auth/SessionRepository.h"

namespace baludesk::core::test {

// Sitzungsablage im Arbeitsspeicher. Gegenstück zu FakeSecretStore, aus
// demselben Grund: Tests über der Ablage sollen weder die echte Datei noch
// den Anmeldeinformationsspeicher des Benutzers anfassen.
class FakeSessionRepository : public SessionRepository {
public:
    std::optional<Session> load() override { return stored; }

    bool save(const Session& session) override {
        ++saveCallCount;
        if (failSave) {
            return false;
        }
        stored = session;
        return true;
    }

    bool saveAccessToken(const std::string& accessToken) override {
        ++saveAccessTokenCallCount;
        if (failSaveAccessToken) {
            return false;
        }
        if (stored) {
            stored->accessToken = accessToken;
        }
        return true;
    }

    bool clear() override {
        ++clearCallCount;
        stored.reset();
        return !failClear;
    }

    std::optional<Session> stored;

    bool failSave = false;
    bool failSaveAccessToken = false;
    bool failClear = false;

    std::size_t saveCallCount = 0;
    std::size_t saveAccessTokenCallCount = 0;
    std::size_t clearCallCount = 0;
};

}  // namespace baludesk::core::test
```

- [ ] **Step 2: Den fehlschlagenden Test schreiben**

Datei `tests/core/authenticated_client_test.cpp`:

```cpp
#include "core/auth/AuthenticatedClient.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "core/api/HttpClient.h"
#include "support/FakeSessionRepository.h"
#include "support/FakeTransport.h"

using baludesk::core::AuthenticatedClient;
using baludesk::core::AuthOutcome;
using baludesk::core::HttpClient;
using baludesk::core::HttpErrorKind;
using baludesk::core::Session;
using baludesk::core::TlsPolicy;
using baludesk::core::TransportResult;
using baludesk::core::test::FakeSessionRepository;
using baludesk::core::test::FakeTransport;

namespace {

TransportResult jsonResult(long statusCode, std::string body) {
    TransportResult result;
    result.error = HttpErrorKind::None;
    result.response.statusCode = statusCode;
    result.response.body = std::move(body);
    return result;
}

TransportResult failure(HttpErrorKind kind, std::string detail) {
    TransportResult result;
    result.error = kind;
    result.detail = std::move(detail);
    return result;
}

Session exampleSession() {
    Session session;
    session.serverUrl = "https://nas.example.com";
    session.username = "sven";
    session.publicKeyPin.clear();
    session.accessToken = "altes-zugangstoken";
    session.refreshToken = "erneuerungstoken";
    return session;
}

const char* kRefreshResponse = R"({
  "access_token": "neues-zugangstoken",
  "token_type": "bearer",
  "user": {"id": 1, "username": "sven"}
})";

struct Fixture {
    std::shared_ptr<FakeTransport> transport = std::make_shared<FakeTransport>();
    FakeSessionRepository repository;
    std::unique_ptr<HttpClient> http;
    std::unique_ptr<AuthenticatedClient> client;

    Fixture() {
        repository.stored = exampleSession();
        http = std::make_unique<HttpClient>("https://nas.example.com", transport, TlsPolicy{});
        client = std::make_unique<AuthenticatedClient>(*http, repository, exampleSession());
    }
};

}  // namespace

TEST(AuthenticatedClientTest, PassesASuccessfulCallThroughUntouched) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, R"({"username":"sven"})"));

    const auto result = f.client->get("/api/auth/me");

    EXPECT_EQ(result.outcome, AuthOutcome::Ok);
    EXPECT_TRUE(result.usable());
    EXPECT_EQ(result.response.statusCode, 200);
    EXPECT_EQ(f.transport->calls.size(), std::size_t{1});
    EXPECT_EQ(f.repository.saveAccessTokenCallCount, std::size_t{0});
}

TEST(AuthenticatedClientTest, SendsTheAccessTokenAsBearer) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, "{}"));

    f.client->get("/api/auth/me");

    ASSERT_EQ(f.transport->calls.size(), std::size_t{1});
    EXPECT_EQ(f.transport->calls[0].request.bearerToken, "altes-zugangstoken");
}

TEST(AuthenticatedClientTest, RefreshesOnceOnA401AndRepeatsTheCall) {
    Fixture f;
    f.transport->queueResult(jsonResult(401, R"({"detail":"Not authenticated"})"));
    f.transport->queueResult(jsonResult(200, kRefreshResponse));
    f.transport->queueResult(jsonResult(200, R"({"username":"sven"})"));

    const auto result = f.client->get("/api/auth/me");

    EXPECT_EQ(result.outcome, AuthOutcome::Refreshed);
    EXPECT_TRUE(result.usable());
    EXPECT_EQ(result.response.statusCode, 200);

    ASSERT_EQ(f.transport->calls.size(), std::size_t{3});
    EXPECT_EQ(f.transport->calls[1].url, "https://nas.example.com/api/auth/refresh");
    // Die Erneuerung geht ohne Bearer und mit dem Erneuerungstoken im Rumpf.
    EXPECT_TRUE(f.transport->calls[1].request.bearerToken.empty());
    EXPECT_NE(f.transport->calls[1].request.jsonBody.find("erneuerungstoken"),
              std::string::npos);
    // Die Wiederholung trägt das neue Zugangstoken, nicht das alte.
    EXPECT_EQ(f.transport->calls[2].request.bearerToken, "neues-zugangstoken");
}

TEST(AuthenticatedClientTest, StoresTheRefreshedAccessToken) {
    Fixture f;
    f.transport->queueResult(jsonResult(401, "{}"));
    f.transport->queueResult(jsonResult(200, kRefreshResponse));
    f.transport->queueResult(jsonResult(200, "{}"));

    f.client->get("/api/auth/me");

    EXPECT_EQ(f.repository.saveAccessTokenCallCount, std::size_t{1});
    ASSERT_TRUE(f.repository.stored.has_value());
    EXPECT_EQ(f.repository.stored->accessToken, "neues-zugangstoken");
    // Das Erneuerungstoken wird vom Server nicht rotiert.
    EXPECT_EQ(f.repository.stored->refreshToken, "erneuerungstoken");
    // Die Datei wird dabei nicht angefasst.
    EXPECT_EQ(f.repository.saveCallCount, std::size_t{0});
    EXPECT_EQ(f.client->session().accessToken, "neues-zugangstoken");
}

TEST(AuthenticatedClientTest, DoesNotRefreshTwiceForOneCall) {
    Fixture f;
    f.transport->queueResult(jsonResult(401, "{}"));
    f.transport->queueResult(jsonResult(200, kRefreshResponse));
    f.transport->queueResult(jsonResult(401, "{}"));

    const auto result = f.client->get("/api/auth/me");

    // Genau ein Erneuerungsversuch je Aufruf. Ohne diese Grenze dreht sich
    // der Client im Kreis: 401 -> erneuern -> 401 -> erneuern -> ...
    ASSERT_EQ(f.transport->calls.size(), std::size_t{3});
    EXPECT_EQ(f.repository.saveAccessTokenCallCount, std::size_t{1});
    // Der Aufruf ist durch, das Ergebnis ist ein 401 - der Aufrufer sieht die
    // rohe Wahrheit statt einer Deutung.
    EXPECT_EQ(result.outcome, AuthOutcome::Ok);
    EXPECT_EQ(result.response.statusCode, 401);
}

TEST(AuthenticatedClientTest, ReportsReauthRequiredWhenTheRefreshTokenIsRejected) {
    Fixture f;
    f.transport->queueResult(jsonResult(401, "{}"));
    f.transport->queueResult(jsonResult(401, R"({"detail":"Invalid refresh token"})"));

    const auto result = f.client->get("/api/auth/me");

    // Das Erneuerungstoken ist tot. Kein Erneuern hilft mehr - nur Neupaaren.
    // Genau diese Unterscheidung von Unreachable verhindert die Schleife.
    EXPECT_EQ(result.outcome, AuthOutcome::ReauthRequired);
    EXPECT_FALSE(result.usable());
    EXPECT_EQ(f.transport->calls.size(), std::size_t{2});
    EXPECT_EQ(f.repository.saveAccessTokenCallCount, std::size_t{0});
}

TEST(AuthenticatedClientTest, ReportsReauthRequiredWhenTheRefreshCarriesNoToken) {
    Fixture f;
    f.transport->queueResult(jsonResult(401, "{}"));
    f.transport->queueResult(jsonResult(200, R"({"token_type":"bearer"})"));

    const auto result = f.client->get("/api/auth/me");

    // 200, aber ohne access_token: damit ist nichts anzufangen, und ein
    // zweiter Versuch brächte dasselbe.
    EXPECT_EQ(result.outcome, AuthOutcome::ReauthRequired);
    EXPECT_EQ(f.client->session().accessToken, "altes-zugangstoken");
}

TEST(AuthenticatedClientTest, ReportsUnreachableWhenTheRefreshCannotBeSent) {
    Fixture f;
    f.transport->queueResult(jsonResult(401, "{}"));
    f.transport->queueResult(failure(HttpErrorKind::Network, "connection refused"));

    const auto result = f.client->get("/api/auth/me");

    // Der Server schläft. Die Sitzung ist unberührt und morgen wieder gut -
    // sie hier zu verwerfen wäre der teuerste denkbare Fehlschluss.
    EXPECT_EQ(result.outcome, AuthOutcome::Unreachable);
    EXPECT_EQ(f.client->session().accessToken, "altes-zugangstoken");
    EXPECT_TRUE(f.repository.stored.has_value());
}

TEST(AuthenticatedClientTest, ReportsUnreachableWhenTheServerErrorsDuringRefresh) {
    Fixture f;
    f.transport->queueResult(jsonResult(401, "{}"));
    f.transport->queueResult(jsonResult(500, R"({"detail":"boom"})"));

    const auto result = f.client->get("/api/auth/me");

    // 5xx ist ein Problem des Servers, keine Aussage über das Token.
    EXPECT_EQ(result.outcome, AuthOutcome::Unreachable);
}

TEST(AuthenticatedClientTest, ReportsTrustBrokenWithoutAttemptingARefresh) {
    Fixture f;
    f.transport->queueResult(failure(HttpErrorKind::CertificatePinMismatch, "pinned key mismatch"));

    const auto result = f.client->get("/api/auth/me");

    // Spec 2.9: harter Abbruch. Ein Erneuerungsversuch über dieselbe
    // Verbindung würde das Erneuerungstoken an eine Gegenstelle schicken,
    // der gerade nicht zu trauen ist.
    EXPECT_EQ(result.outcome, AuthOutcome::TrustBroken);
    EXPECT_EQ(f.transport->calls.size(), std::size_t{1});
}

TEST(AuthenticatedClientTest, LeavesOtherStatusCodesAlone) {
    Fixture f;
    f.transport->queueResult(jsonResult(403, R"({"detail":"Forbidden"})"));

    const auto result = f.client->get("/api/files/list");

    // 403 heisst „darfst du nicht", nicht „bist du nicht". Erneuern ändert
    // daran nichts.
    EXPECT_EQ(result.outcome, AuthOutcome::Ok);
    EXPECT_EQ(result.response.statusCode, 403);
    EXPECT_EQ(f.transport->calls.size(), std::size_t{1});
}

TEST(AuthenticatedClientTest, PostJsonTakesTheSamePath) {
    Fixture f;
    f.transport->queueResult(jsonResult(401, "{}"));
    f.transport->queueResult(jsonResult(200, kRefreshResponse));
    f.transport->queueResult(jsonResult(200, R"({"changes":[]})"));

    const auto result = f.client->postJson("/api/sync/changes", R"({"since":0})");

    EXPECT_EQ(result.outcome, AuthOutcome::Refreshed);
    ASSERT_EQ(f.transport->calls.size(), std::size_t{3});
    EXPECT_EQ(f.transport->calls[2].url, "https://nas.example.com/api/sync/changes");
    EXPECT_EQ(f.transport->calls[2].request.jsonBody, R"({"since":0})");
    EXPECT_EQ(f.transport->calls[2].request.bearerToken, "neues-zugangstoken");
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
    core/session_paths_test.cpp
    core/trust_negotiation_test.cpp
    core/authenticated_client_test.cpp
)
```

- [ ] **Step 4: Test laufen lassen und Fehlschlag bestätigen**

```powershell
cmake --preset msvc2022
cmake --build --preset debug
```

Erwartet: FEHLER beim Übersetzen — `Cannot open include file: 'core/auth/AuthenticatedClient.h'`.

- [ ] **Step 5: `src/core/auth/AuthenticatedClient.h` schreiben**

```cpp
#pragma once

#include <string>

#include "core/api/HttpClient.h"
#include "core/auth/Session.h"

namespace baludesk::core {

class SessionRepository;

// Was ein Aufruf über den Zustand der Sitzung aussagt - unabhängig davon, was
// der Server inhaltlich geantwortet hat.
enum class AuthOutcome {
    // Der Aufruf ist durch. Der Statuscode in der Antwort sagt den Rest; er
    // darf durchaus 403 oder 404 sein.
    Ok,
    // Wie Ok, aber das Zugangstoken war abgelaufen und wurde unterwegs
    // erneuert. Der Aufrufer muss nichts tun; die Unterscheidung ist für das
    // Protokoll und für Anzeigen da.
    Refreshed,
    // Die Erneuerung wurde abgelehnt. Kein weiterer Versuch hilft, nur
    // Neupaaren. Von Unreachable zu unterscheiden ist der ganze Punkt: sonst
    // verwirft der Client eine gute Sitzung, weil der Server gerade schläft.
    ReauthRequired,
    // Das Zertifikat der Gegenstelle passt nicht mehr. Spec Abschnitt 2.9:
    // harter Abbruch, keine Nachfrage im laufenden Betrieb.
    TrustBroken,
    // Netz weg, Zeitlimit, Serverfehler. Die Sitzung ist unberührt; später
    // erneut versuchen.
    Unreachable
};

struct AuthenticatedResult {
    HttpResult response;
    AuthOutcome outcome = AuthOutcome::Ok;

    // Es kam eine Antwort des Servers zurück, die sich auswerten lässt.
    [[nodiscard]] bool usable() const {
        return outcome == AuthOutcome::Ok || outcome == AuthOutcome::Refreshed;
    }
};

// Ein HttpClient, der die Sitzung mitführt und ein abgelaufenes Zugangstoken
// still erneuert.
//
// Die eine Regel, die diese Klasse trägt: **genau ein Erneuerungsversuch je
// Aufruf.** Antwortet der Server auch auf die Wiederholung mit 401, ist das
// Ergebnis ein durchgereichter 401 und keine zweite Erneuerung. Ohne diese
// Grenze dreht sich der Client im Kreis.
//
// Das Erneuerungstoken wird vom Server nicht rotiert; erneuert wird
// ausschliesslich das Zugangstoken, abgelegt über
// SessionRepository::saveAccessToken().
//
// Die Klasse besitzt weder den HttpClient noch das Repository; beide müssen
// sie überleben.
class AuthenticatedClient {
public:
    AuthenticatedClient(HttpClient& http, SessionRepository& repository, Session session);

    AuthenticatedResult get(const std::string& path);
    AuthenticatedResult postJson(const std::string& path, const std::string& jsonBody);

    // Der aktuelle Stand, einschliesslich eines unterwegs erneuerten
    // Zugangstokens.
    [[nodiscard]] const Session& session() const { return session_; }

private:
    AuthenticatedResult dispatch(HttpMethod method,
                                 const std::string& path,
                                 const std::string& jsonBody);

    HttpResult perform(HttpMethod method, const std::string& path, const std::string& jsonBody);

    // Ok, wenn ein neues Zugangstoken vorliegt; sonst der Grund.
    AuthOutcome refresh();

    HttpClient& http_;
    SessionRepository& repository_;
    Session session_;
};

}  // namespace baludesk::core
```

- [ ] **Step 6: `src/core/auth/AuthenticatedClient.cpp` schreiben**

```cpp
#include "core/auth/AuthenticatedClient.h"

#include <nlohmann/json.hpp>

#include <utility>

#include "core/auth/SessionRepository.h"
#include "core/util/Logger.h"

namespace baludesk::core {
namespace {

constexpr const char* kRefreshPath = "/api/auth/refresh";

// Was ein Transportfehler für die Sitzung bedeutet.
AuthOutcome outcomeFor(HttpErrorKind error) {
    switch (error) {
        case HttpErrorKind::None:
            return AuthOutcome::Ok;
        case HttpErrorKind::CertificatePinMismatch:
        case HttpErrorKind::CertificateUntrusted:
            // Beide heissen: dieser Gegenstelle ist gerade nicht zu trauen.
            // Ob der angeheftete Schlüssel abweicht oder ein Zertifikat ohne
            // Pin abgelaufen ist, ändert nichts an der Folge - es wird nicht
            // weitergeredet und im laufenden Betrieb auch nicht nachgefragt.
            return AuthOutcome::TrustBroken;
        case HttpErrorKind::Network:
        case HttpErrorKind::Timeout:
        case HttpErrorKind::Protocol:
            return AuthOutcome::Unreachable;
    }
    return AuthOutcome::Unreachable;
}

std::string accessTokenFrom(const std::string& body) {
    auto document = nlohmann::json::parse(body, nullptr, false);
    if (document.is_discarded() || !document.is_object()) {
        return {};
    }
    const auto it = document.find("access_token");
    if (it == document.end() || !it->is_string()) {
        return {};
    }
    return it->get<std::string>();
}

}  // namespace

AuthenticatedClient::AuthenticatedClient(HttpClient& http,
                                         SessionRepository& repository,
                                         Session session)
    : http_(http), repository_(repository), session_(std::move(session)) {}

HttpResult AuthenticatedClient::perform(HttpMethod method,
                                        const std::string& path,
                                        const std::string& jsonBody) {
    if (method == HttpMethod::Post) {
        return http_.postJson(path, jsonBody, session_.accessToken);
    }
    return http_.get(path, session_.accessToken);
}

AuthOutcome AuthenticatedClient::refresh() {
    nlohmann::json request;
    request["refresh_token"] = session_.refreshToken;

    // Ohne Bearer: die Erneuerung weist sich über das Erneuerungstoken im
    // Rumpf aus, und das abgelaufene Zugangstoken mitzuschicken hätte
    // bestenfalls keine Wirkung.
    const HttpResult result = http_.postJson(kRefreshPath, request.dump());

    const AuthOutcome transport = outcomeFor(result.error);
    if (transport != AuthOutcome::Ok) {
        Logger::debug("Sitzung: Erneuerung nicht zustellbar — {}", result.detail);
        return transport;
    }

    if (result.statusCode == 401 || result.statusCode == 403) {
        Logger::warn("Sitzung: Erneuerungstoken abgelehnt (Status {})", result.statusCode);
        return AuthOutcome::ReauthRequired;
    }

    if (!result.ok()) {
        // 5xx und alles Übrige: ein Problem des Servers, keine Aussage über
        // das Token.
        Logger::warn("Sitzung: Erneuerung mit Status {} fehlgeschlagen", result.statusCode);
        return AuthOutcome::Unreachable;
    }

    const std::string token = accessTokenFrom(result.body);
    if (token.empty()) {
        Logger::error("Sitzung: Erneuerung ohne access_token beantwortet");
        return AuthOutcome::ReauthRequired;
    }

    session_.accessToken = token;

    if (!repository_.saveAccessToken(token)) {
        // Nicht abbrechen: der Aufruf, der gleich folgt, arbeitet mit dem
        // Token im Speicher und gelingt. Verloren ist allein die Haltbarkeit
        // über das Programmende hinaus - beim nächsten Start wird eben erneut
        // erneuert.
        Logger::error("Sitzung: erneuertes Zugangstoken konnte nicht abgelegt werden");
    }

    Logger::info("Sitzung: Zugangstoken erneuert");
    return AuthOutcome::Ok;
}

AuthenticatedResult AuthenticatedClient::dispatch(HttpMethod method,
                                                  const std::string& path,
                                                  const std::string& jsonBody) {
    AuthenticatedResult result;
    result.response = perform(method, path, jsonBody);
    result.outcome = outcomeFor(result.response.error);

    if (result.outcome != AuthOutcome::Ok) {
        return result;
    }
    if (result.response.statusCode != 401) {
        // Durch. Auch ein 403 oder 404 ist eine Antwort, keine Frage an die
        // Sitzung.
        return result;
    }

    const AuthOutcome refreshOutcome = refresh();
    if (refreshOutcome != AuthOutcome::Ok) {
        result.outcome = refreshOutcome;
        return result;
    }

    // Der eine erlaubte zweite Versuch.
    result.response = perform(method, path, jsonBody);
    result.outcome = outcomeFor(result.response.error);
    if (result.outcome == AuthOutcome::Ok && result.response.ok()) {
        result.outcome = AuthOutcome::Refreshed;
    }
    return result;
}

AuthenticatedResult AuthenticatedClient::get(const std::string& path) {
    return dispatch(HttpMethod::Get, path, {});
}

AuthenticatedResult AuthenticatedClient::postJson(const std::string& path,
                                                  const std::string& jsonBody) {
    return dispatch(HttpMethod::Post, path, jsonBody);
}

}  // namespace baludesk::core
```

- [ ] **Step 7: `src/core/CMakeLists.txt` erweitern**

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
    api/TrustNegotiation.cpp
    auth/PairingClient.cpp
    auth/AuthenticatedClient.cpp
    auth/FileSessionRepository.cpp
    auth/SessionPaths.cpp
)
```

- [ ] **Step 8: Tests laufen lassen und grün bestätigen**

```powershell
cmake --preset msvc2022
cmake --build --preset debug
ctest --preset debug
```

Erwartet: **153 Tests, alle bestanden** (142 plus 11 neue).

- [ ] **Step 9: Committen**

```powershell
git add src/core/auth/AuthenticatedClient.h src/core/auth/AuthenticatedClient.cpp src/core/CMakeLists.txt tests/core/authenticated_client_test.cpp tests/core/support/FakeSessionRepository.h tests/CMakeLists.txt
git commit -m @'
feat(core): AuthenticatedClient erneuert genau einmal je Aufruf

401 -> erneuern -> genau eine Wiederholung. Antwortet der Server auch darauf
mit 401, wird der Status durchgereicht statt ein zweites Mal erneuert - sonst
dreht sich der Client im Kreis.

ReauthRequired ist von Unreachable getrennt: ein totes Erneuerungstoken
verlangt Neupaaren, ein schlafender Server verlangt Geduld. Beides sieht an
der Aufrufstelle gleich aus, wenn man es nicht trennt.
'@
```

---

## Task 4: Gerätekennung — einmal erzeugt, überlebt das Abmelden

Der Server führt aus `device_id` seine Geräteliste in der Web-Oberfläche. Das Prüfprogramm hat `"baludesk-probe"` fest verdrahtet; die Anwendung braucht einen Wert, der der Installation gehört und nicht der Bindung an einen Server.

**Files:**
- Modify: `src/app/SettingsStore.h`
- Modify: `src/app/SettingsStore.cpp`
- Test: `tests/app/settings_store_test.cpp` (ergänzen)

**Interfaces:**
- Consumes: nichts aus früheren Tasks
- Produces:
  - `QString baludesk::app::SettingsStore::deviceId()` — **nicht** `const`, weil der erste Aufruf schreibt
  - `static QString baludesk::app::SettingsStore::deviceName()`

---

- [ ] **Step 1: Die fehlschlagenden Tests schreiben**

An `tests/app/settings_store_test.cpp` anhängen:

```cpp
TEST(SettingsStoreTest, GeneratesADeviceIdOnFirstUse) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    SettingsStore store(directory.filePath(QStringLiteral("settings.ini")));

    const QString id = store.deviceId();
    EXPECT_FALSE(id.isEmpty());
    // Ohne geschweifte Klammern: die sind Windows-Kosmetik, und der Server
    // führt die Kennung als schlichte Zeichenkette.
    EXPECT_FALSE(id.startsWith(QLatin1Char('{')));
}

TEST(SettingsStoreTest, KeepsTheDeviceIdAcrossInstances) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.ini"));

    QString first;
    {
        SettingsStore store(path);
        first = store.deviceId();
    }

    SettingsStore reopened(path);
    // Läge die Kennung in der Sitzung, bekäme jedes Neupaaren einen neuen
    // Eintrag in der Geräteliste des Servers.
    EXPECT_EQ(reopened.deviceId(), first);
}

TEST(SettingsStoreTest, KeepsTheDeviceIdWhenSettingsAreSaved) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.ini"));

    SettingsStore store(path);
    const QString id = store.deviceId();

    AppConfig config;
    config.pollIntervalSeconds = 90;
    store.save(config);

    EXPECT_EQ(store.deviceId(), id);
    EXPECT_EQ(store.load().pollIntervalSeconds, 90);
}

TEST(SettingsStoreTest, DeviceNameIsNeverEmpty) {
    // Der Name erscheint in der Geräteliste. Ein leerer Eintrag dort wäre
    // nicht zuzuordnen.
    EXPECT_FALSE(SettingsStore::deviceName().isEmpty());
}
```

- [ ] **Step 2: Tests laufen lassen und Fehlschlag bestätigen**

```powershell
cmake --build --preset debug
```

Erwartet: FEHLER beim Übersetzen — `'deviceId': is not a member of 'baludesk::app::SettingsStore'`.

- [ ] **Step 3: `src/app/SettingsStore.h` erweitern**

Im `public`-Abschnitt hinter `save()` ergänzen:

```cpp
    // Die Kennung dieser Installation, wie sie beim Paaren als device_id
    // mitgeht. Beim ersten Aufruf erzeugt und abgelegt, danach unverändert.
    //
    // Absichtlich NICHT in der Sitzung: der Server führt daraus seine
    // Geräteliste in der Web-Oberfläche. Läge sie in der Sitzung, bekäme
    // jedes Neupaaren einen neuen Eintrag, und das Aufräumen wäre Handarbeit
    // im Browser. Die Kennung gehört der Installation, nicht der Bindung an
    // einen Server - und überlebt deshalb das Abmelden.
    //
    // Nicht const: der erste Aufruf schreibt.
    [[nodiscard]] QString deviceId();

    // Der Name, unter dem das Gerät in der Web-Oberfläche erscheint.
    [[nodiscard]] static QString deviceName();
```

- [ ] **Step 4: `src/app/SettingsStore.cpp` erweitern**

Oben die Includes ergänzen:

```cpp
#include <QSysInfo>
#include <QUuid>
```

Im anonymen Namensraum bei den übrigen Schlüsseln ergänzen:

```cpp
constexpr const char* kDeviceId = "device/id";
```

Am Ende des Namensraums `baludesk::app`, hinter `save()`, einfügen:

```cpp
QString SettingsStore::deviceId() {
    const QString stored = settings_.value(kDeviceId).toString();
    if (!stored.isEmpty()) {
        return stored;
    }

    // QUuid::createUuid() zieht aus dem Zufallsgenerator des Systems.
    const QString created = QUuid::createUuid().toString(QUuid::WithoutBraces);
    settings_.setValue(kDeviceId, created);
    settings_.sync();
    return created;
}

QString SettingsStore::deviceName() {
    const QString host = QSysInfo::machineHostName();
    // Ein leerer Name wäre in der Geräteliste des Servers nicht zuzuordnen.
    return host.isEmpty() ? QStringLiteral("BaluDesk") : host;
}
```

- [ ] **Step 5: Tests laufen lassen und grün bestätigen**

```powershell
cmake --build --preset debug
ctest --preset debug
```

Erwartet: **157 Tests, alle bestanden** (153 plus 4 neue).

- [ ] **Step 6: Committen**

```powershell
git add src/app/SettingsStore.h src/app/SettingsStore.cpp tests/app/settings_store_test.cpp
git commit -m @'
feat(app): Geraetekennung einmal erzeugen und behalten

Sie gehoert der Installation, nicht der Bindung an einen Server, und liegt
deshalb in den Einstellungen statt in der Sitzung - sonst waechst die
Geraeteliste im Browser mit jedem Neupaaren um einen Eintrag.
'@
```

---

## Task 5: AuthBridge — Gerüst, Sitzung wiederherstellen, Abmelden

Die einzige Stelle im Programm, an der beide Welten sich berühren (Spec Abschnitt 5). Sie entsteht in diesem Task samt Bibliothek und Build-Verdrahtung; das Paaren kommt in Task 6 dazu.

**`AuthBridge` erzeugt keinen Thread und kennt keinen.** Wo das Objekt lebt, entscheidet allein sein Eigentümer — in der Anwendung ist das `MainWindow` (Task 8), in den Tests der Test selbst. Damit ist die Klasse ohne Ereignisschleife prüfbar, und die Threading-Entscheidung liegt an einer einzigen, sichtbaren Stelle.

**Files:**
- Create: `src/bridge/AuthBridge.h`
- Create: `src/bridge/AuthBridge.cpp`
- Create: `src/bridge/CMakeLists.txt`
- Modify: `CMakeLists.txt` (Wurzel)
- Modify: `src/app/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Test: `tests/bridge/auth_bridge_test.cpp`

**Interfaces:**
- Consumes: `TrustNegotiation` (Task 1), `SessionRepository::clear` (Task 2), `AuthenticatedClient` (Task 3), sowie `HttpClient`, `Transport`, `SecretStore`, `FileSessionRepository`, `checkServerUrl`, `PairingClient` aus früheren Stufen
- Produces:
  - `class baludesk::bridge::AuthBridge : public QObject` mit dem Konstruktor
    `AuthBridge(std::shared_ptr<core::Transport> transport, std::unique_ptr<core::SecretStore> secrets, std::string sessionPath, QString deviceId, QString deviceName, QObject* parent = nullptr)`
  - Slots: `restoreSession()`, `connectToServer(const QString&)`, `confirmCertificate()`, `rejectCertificate()`, `cancelPairing()`, `pollNow()`, `logout()`
  - Signale: `noSession()`, `sessionEstablished(QString, QString)`, `serverUnreachable(QString)`, `serverUrlRejected(QString)`, `certificateConfirmationRequired(QString, QString)`, `certificateChanged(QString, QString)`, `pairingCodeReady(QString, QString, int)`, `pairingNotice(QString)`, `pairingFailed(QString)`, `pairingCancelled()`, `reauthRequired(QString)`, `loggedOut()`, `logoutIncomplete(QString)`
  - CMake-Ziel `baludesk_bridge`

---

- [ ] **Step 1: `src/bridge/CMakeLists.txt` anlegen**

```cmake
# Diese Schicht darf Qt kennen, aber KEINE Widgets.
#
# baludesk_app_lib linkt Qt6::Widgets. Läge AuthBridge dort, könnte jederzeit
# eine QMessageBox in den Arbeiterthread rutschen - Widgets sind ausserhalb des
# GUI-Threads verboten, und der Fehler zeigt sich erst zur Laufzeit, an
# irgendeiner Stelle. Eine Bibliothek, die nur Qt6::Core linkt, macht daraus
# einen Übersetzungsfehler.
find_package(Qt6 6.8 REQUIRED COMPONENTS Core)

qt_standard_project_setup()

add_library(baludesk_bridge STATIC
    AuthBridge.cpp
)

target_include_directories(baludesk_bridge PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(baludesk_bridge PUBLIC Qt6::Core baludesk_core)
```

- [ ] **Step 2: Die Wurzel-`CMakeLists.txt` erweitern**

`add_subdirectory(src/bridge)` zwischen `src/core` und `src/app` einfügen — die Reihenfolge zählt, weil `src/app` gleich gegen `baludesk_bridge` linkt:

```cmake
add_subdirectory(src/core)
add_subdirectory(src/bridge)
add_subdirectory(src/app)
add_subdirectory(tests)
if(BALUDESK_BUILD_TOOLS)
    add_subdirectory(tools/probe)
endif()
```

Der `get_target_property`-Block darunter bleibt unverändert. Er prüft weiterhin nur `baludesk_core` — genau richtig: `baludesk_bridge` **darf** Qt linken.

- [ ] **Step 3: `src/app/CMakeLists.txt` gegen die Bridge linken**

Die eine Zeile ersetzen, alles Übrige unverändert lassen:

```cmake
target_link_libraries(baludesk_app_lib PUBLIC Qt6::Widgets baludesk_core baludesk_bridge)
```

- [ ] **Step 4: Den fehlschlagenden Test schreiben**

Datei `tests/bridge/auth_bridge_test.cpp`:

```cpp
#include "bridge/AuthBridge.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>

#include <memory>
#include <string>
#include <utility>

#include "core/api/HttpTypes.h"
#include "support/FakeSecretStore.h"
#include "support/FakeTransport.h"

using baludesk::bridge::AuthBridge;
using baludesk::core::HttpErrorKind;
using baludesk::core::TransportResult;
using baludesk::core::test::FakeSecretStore;
using baludesk::core::test::FakeTransport;

namespace {

// QObject und QTimer verlangen keine laufende Ereignisschleife, aber ohne eine
// QCoreApplication-Instanz warnt Qt beim Anlegen eines Zeitgebers. Eine
// einzige, statisch gehaltene Instanz für alle Tests - eine zweite anzulegen
// wäre ein fataler Fehler in Qt.
QCoreApplication& application() {
    static int argc = 1;
    static char argName[] = "baludesk_app_tests";
    static char* argv[] = {argName, nullptr};
    static QCoreApplication instance(argc, argv);
    return instance;
}

TransportResult jsonResult(long statusCode, std::string body) {
    TransportResult result;
    result.error = HttpErrorKind::None;
    result.response.statusCode = statusCode;
    result.response.body = std::move(body);
    return result;
}

TransportResult failure(HttpErrorKind kind, std::string detail) {
    TransportResult result;
    result.error = kind;
    result.detail = std::move(detail);
    return result;
}

// Legt eine vollständige Sitzung in Datei und Geheimnisspeicher ab, wie sie
// nach einer echten Paarung dastünde.
void storeSession(const std::string& jsonPath, FakeSecretStore& secrets) {
    baludesk::core::Session session;
    session.serverUrl = "https://nas.example.com";
    session.username = "sven";
    session.publicKeyPin.clear();
    session.accessToken = "zugangstoken";
    session.refreshToken = "erneuerungstoken";

    baludesk::core::FileSessionRepository repository(jsonPath, secrets);
    ASSERT_TRUE(repository.save(session));
}

// Hält alles beisammen, was eine Bridge zum Leben braucht, und gibt den
// Geheimnisspeicher zusätzlich als rohe Referenz heraus - die Bridge besitzt
// ihn, der Test will ihn trotzdem befragen.
struct Fixture {
    QTemporaryDir directory;
    std::shared_ptr<FakeTransport> transport = std::make_shared<FakeTransport>();
    FakeSecretStore* secrets = nullptr;
    std::unique_ptr<AuthBridge> bridge;

    Fixture() {
        application();
        EXPECT_TRUE(directory.isValid());

        auto owned = std::make_unique<FakeSecretStore>();
        secrets = owned.get();

        bridge = std::make_unique<AuthBridge>(transport,
                                              std::move(owned),
                                              sessionPath(),
                                              QStringLiteral("geraet-1"),
                                              QStringLiteral("Svens Rechner"));
    }

    [[nodiscard]] std::string sessionPath() const {
        return directory.filePath(QStringLiteral("session.json")).toStdString();
    }
};

}  // namespace

TEST(AuthBridgeTest, ReportsNoSessionWhenNothingIsStored) {
    Fixture f;
    QSignalSpy none(f.bridge.get(), &AuthBridge::noSession);

    f.bridge->restoreSession();

    EXPECT_EQ(none.count(), 1);
    // Ohne Sitzung gibt es nichts zu fragen - der Server wird nicht behelligt.
    EXPECT_TRUE(f.transport->calls.empty());
}

TEST(AuthBridgeTest, EstablishesTheStoredSessionWhenTheServerAcceptsIt) {
    Fixture f;
    storeSession(f.sessionPath(), *f.secrets);
    f.transport->queueResult(jsonResult(200, R"({"username":"sven"})"));

    QSignalSpy established(f.bridge.get(), &AuthBridge::sessionEstablished);
    f.bridge->restoreSession();

    ASSERT_EQ(established.count(), 1);
    EXPECT_EQ(established.at(0).at(0).toString(), QStringLiteral("https://nas.example.com"));
    EXPECT_EQ(established.at(0).at(1).toString(), QStringLiteral("sven"));

    ASSERT_EQ(f.transport->calls.size(), std::size_t{1});
    EXPECT_EQ(f.transport->calls[0].url, "https://nas.example.com/api/auth/me");
}

TEST(AuthBridgeTest, AsksToReauthenticateWhenTheRefreshTokenIsDead) {
    Fixture f;
    storeSession(f.sessionPath(), *f.secrets);
    f.transport->queueResult(jsonResult(401, "{}"));
    f.transport->queueResult(jsonResult(401, R"({"detail":"Invalid refresh token"})"));

    QSignalSpy reauth(f.bridge.get(), &AuthBridge::reauthRequired);
    QSignalSpy established(f.bridge.get(), &AuthBridge::sessionEstablished);
    f.bridge->restoreSession();

    EXPECT_EQ(established.count(), 0);
    ASSERT_EQ(reauth.count(), 1);
    EXPECT_FALSE(reauth.at(0).at(0).toString().isEmpty());
}

TEST(AuthBridgeTest, KeepsTheSessionWhenTheServerIsAsleep) {
    Fixture f;
    storeSession(f.sessionPath(), *f.secrets);
    f.transport->queueResult(failure(HttpErrorKind::Network, "connection refused"));

    QSignalSpy unreachable(f.bridge.get(), &AuthBridge::serverUnreachable);
    QSignalSpy reauth(f.bridge.get(), &AuthBridge::reauthRequired);
    f.bridge->restoreSession();

    // Ein schlafender Server ist laut Spec Abschnitt 10 Betriebszustand. Die
    // Sitzung deshalb zu verwerfen wäre der teuerste denkbare Fehlschluss.
    EXPECT_EQ(unreachable.count(), 1);
    EXPECT_EQ(reauth.count(), 0);
    EXPECT_TRUE(f.secrets->contains("session/access-token"));
}

TEST(AuthBridgeTest, ReportsAChangedCertificate) {
    Fixture f;
    storeSession(f.sessionPath(), *f.secrets);
    f.transport->queueResult(failure(HttpErrorKind::CertificatePinMismatch, "pinned key mismatch"));

    QSignalSpy changed(f.bridge.get(), &AuthBridge::certificateChanged);
    f.bridge->restoreSession();

    ASSERT_EQ(changed.count(), 1);
    EXPECT_EQ(changed.at(0).at(0).toString(), QStringLiteral("https://nas.example.com"));
    EXPECT_FALSE(changed.at(0).at(1).toString().isEmpty());
    // Kein zweiter Aufruf: keine Erneuerung über eine Verbindung, der gerade
    // nicht zu trauen ist.
    EXPECT_EQ(f.transport->calls.size(), std::size_t{1});
}

TEST(AuthBridgeTest, LogoutClearsTheSessionAndSaysSo) {
    Fixture f;
    storeSession(f.sessionPath(), *f.secrets);

    QSignalSpy loggedOut(f.bridge.get(), &AuthBridge::loggedOut);
    f.bridge->logout();

    EXPECT_EQ(loggedOut.count(), 1);
    EXPECT_TRUE(f.secrets->entries.empty());
    EXPECT_FALSE(QFile::exists(f.directory.filePath(QStringLiteral("session.json"))));
}

TEST(AuthBridgeTest, LogoutSaysSoWhenSomethingRemained) {
    Fixture f;
    storeSession(f.sessionPath(), *f.secrets);
    f.secrets->failOnEraseNumber = 1;

    QSignalSpy incomplete(f.bridge.get(), &AuthBridge::logoutIncomplete);
    QSignalSpy loggedOut(f.bridge.get(), &AuthBridge::loggedOut);
    f.bridge->logout();

    // „Erledigt" zu melden, während ein Token im Speicher steht, wäre eine
    // Unwahrheit an genau der Stelle, an der jemand seine Spuren löschen will.
    EXPECT_EQ(loggedOut.count(), 0);
    ASSERT_EQ(incomplete.count(), 1);
    EXPECT_FALSE(incomplete.at(0).at(0).toString().isEmpty());
}
```

Der Test bindet `QFile` und `FileSessionRepository` ein — die Include-Liste oben um `#include <QFile>` und `#include "core/auth/FileSessionRepository.h"` ergänzen.

- [ ] **Step 5: `tests/CMakeLists.txt` erweitern**

Drei Änderungen. Erstens die `find_package`-Zeile für Qt um `Test` erweitern (`QSignalSpy` steckt im Qt-Test-Modul):

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Test)
```

Zweitens die neue Testdatei aufnehmen:

```cmake
add_executable(baludesk_app_tests
    app/settings_store_test.cpp
    app/qt_log_bridge_test.cpp
    bridge/auth_bridge_test.cpp
)
```

Drittens den Block darunter ersetzen, damit die App-Tests `Qt6::Test` bekommen und die Testhilfen unter `support/` finden:

```cmake
target_link_libraries(baludesk_app_tests PRIVATE
    baludesk_app_lib
    Qt6::Test
    GTest::gtest
    GTest::gtest_main
)

# Wie bei baludesk_core_tests: erlaubt support/FakeTransport.h als vollen Pfad
# ab tests/ einzubinden. Die Bridge-Tests benutzen dieselben Testhilfen wie die
# Kern-Tests - sie doppelt vorzuhalten hiesse, sie doppelt zu pflegen.
target_include_directories(baludesk_app_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
```

- [ ] **Step 6: Test laufen lassen und Fehlschlag bestätigen**

```powershell
cmake --preset msvc2022
cmake --build --preset debug
```

Erwartet: FEHLER beim Übersetzen — `Cannot open include file: 'bridge/AuthBridge.h'`.

- [ ] **Step 7: `src/bridge/AuthBridge.h` schreiben**

```cpp
#pragma once

#include <QObject>
#include <QString>

#include <memory>
#include <string>

#include "core/api/TrustNegotiation.h"
#include "core/auth/AuthenticatedClient.h"
#include "core/auth/PairingClient.h"
#include "core/auth/SessionRepository.h"
#include "core/util/SecretStore.h"

class QTimer;

namespace baludesk::core {
class HttpClient;
class Transport;
}  // namespace baludesk::core

namespace baludesk::bridge {

// Die einzige Stelle im Programm, an der beide Welten sich berühren
// (Spec Abschnitt 5).
//
// Sie besitzt alles, was blockiert: HttpClient blockiert bis zu 30 Sekunden
// und lässt sich nicht abbrechen (Spec 2.10). Die Oberfläche redet
// ausschliesslich über Signale und Slots mit ihr und fasst nie einen
// core-Typ an, der ins Netz greift.
//
// DIESE KLASSE ERZEUGT KEINEN THREAD UND KENNT KEINEN. Wo sie lebt,
// entscheidet ihr Eigentümer - in der Anwendung schiebt MainWindow sie per
// moveToThread in einen eigenen QThread, in den Tests bleibt sie, wo sie ist.
// Dadurch liegt die Threading-Entscheidung an einer einzigen sichtbaren
// Stelle, und die Klasse ist ohne Ereignisschleife prüfbar.
//
// Ebenfalls bewusst: keine Widgets. Diese Bibliothek linkt Qt6::Core, nicht
// Qt6::Widgets - ein QMessageBox im Arbeiterthread wäre sonst ein Fehler, der
// sich erst zur Laufzeit zeigt.
class AuthBridge : public QObject {
    Q_OBJECT

public:
    // transport wird geteilt (HttpClient hält ihn als shared_ptr); secrets
    // geht in den Besitz der Bridge über.
    AuthBridge(std::shared_ptr<core::Transport> transport,
               std::unique_ptr<core::SecretStore> secrets,
               std::string sessionPath,
               QString deviceId,
               QString deviceName,
               QObject* parent = nullptr);

    ~AuthBridge() override;

public slots:
    // Beim Start: abgelegte Sitzung laden und beim Server nachfragen, ob sie
    // noch trägt. Meldet genau eines von noSession, sessionEstablished,
    // reauthRequired, certificateChanged oder serverUnreachable.
    void restoreSession();

    // Adresse prüfen, Vertrauen klären, Paarung beginnen.
    void connectToServer(const QString& rawUrl);

    // Nach certificateConfirmationRequired: den gezeigten Schlüssel anheften
    // und weitermachen.
    void confirmCertificate();

    // Nach certificateConfirmationRequired: nichts anheften, Vorgang beenden.
    void rejectCertificate();

    // Während der Codeanzeige: Vorgang beenden. Der Gerätecode verfällt beim
    // Server von selbst, es ist nichts zurückzunehmen.
    void cancelPairing();

    // Ein Abfrageschritt. Wird vom Zeitgeber aufgerufen.
    //
    // Öffentlich, damit die Tests den Ablauf ohne Ereignisschleife fahren
    // können - sonst wäre dieser Schritt nur über echte verstreichende
    // Sekunden erreichbar, und die Abfragefolge bliebe ungeprüft.
    void pollNow();

    // Sitzung entfernen. Meldet loggedOut oder logoutIncomplete.
    void logout();

signals:
    void noSession();
    void sessionEstablished(const QString& serverUrl, const QString& username);
    void serverUnreachable(const QString& message);
    void serverUrlRejected(const QString& message);
    void certificateConfirmationRequired(const QString& fingerprint, const QString& reason);

    // Der angeheftete Schlüssel passt nicht mehr, oder ein Zertifikat ohne
    // Pin lässt sich nicht mehr prüfen. Beides endet auf derselben Seite,
    // braucht dort aber verschiedene Sätze - daher der Grund als zweites
    // Argument.
    void certificateChanged(const QString& serverUrl, const QString& reason);

    void pairingCodeReady(const QString& userCode,
                          const QString& verificationUrl,
                          int expiresInSeconds);
    void pairingNotice(const QString& message);
    void pairingFailed(const QString& message);
    void pairingCancelled();
    void reauthRequired(const QString& message);
    void loggedOut();
    void logoutIncomplete(const QString& message);

private:
    void beginPairing();
    void finishPairing();
    void resetPairing();

    std::shared_ptr<core::Transport> transport_;

    // REIHENFOLGE IST TRAGEND: FileSessionRepository hält eine SecretStore&.
    // Mitglieder werden in umgekehrter Deklarationsreihenfolge zerstört, also
    // stirbt repository_ vor secrets_. Wer die beiden Zeilen tauscht, baut
    // eine Referenz, die ihren Bezug überlebt - und zwar still.
    std::unique_ptr<core::SecretStore> secrets_;
    std::unique_ptr<core::SessionRepository> repository_;

    QString deviceId_;
    QString deviceName_;

    std::unique_ptr<core::HttpClient> http_;
    std::unique_ptr<core::TrustNegotiation> trust_;
    std::unique_ptr<core::PairingClient> pairing_;
    std::unique_ptr<core::AuthenticatedClient> authenticated_;

    QTimer* pollTimer_ = nullptr;
    QString pendingServerUrl_;
};

}  // namespace baludesk::bridge
```

- [ ] **Step 8: `src/bridge/AuthBridge.cpp` schreiben — Konstruktor, Wiederherstellen, Abmelden**

Das Paaren bleibt in diesem Task bewusst leer und kommt in Task 6 dazu.

```cpp
#include "bridge/AuthBridge.h"

#include <QTimer>

#include <chrono>
#include <cstdint>
#include <utility>

#include "core/api/HttpClient.h"
#include "core/api/ServerUrl.h"
#include "core/api/TlsPolicy.h"
#include "core/auth/FileSessionRepository.h"
#include "core/util/Logger.h"

namespace baludesk::bridge {
namespace {

std::int64_t nowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

}  // namespace

AuthBridge::AuthBridge(std::shared_ptr<core::Transport> transport,
                       std::unique_ptr<core::SecretStore> secrets,
                       std::string sessionPath,
                       QString deviceId,
                       QString deviceName,
                       QObject* parent)
    : QObject(parent),
      transport_(std::move(transport)),
      secrets_(std::move(secrets)),
      deviceId_(std::move(deviceId)),
      deviceName_(std::move(deviceName)) {
    repository_ =
        std::make_unique<core::FileSessionRepository>(std::move(sessionPath), *secrets_);

    // Mit this als Eltern: der Zeitgeber wandert bei einem moveToThread mit
    // und gehört danach demselben Thread wie alles Übrige.
    pollTimer_ = new QTimer(this);
    pollTimer_->setSingleShot(true);
    connect(pollTimer_, &QTimer::timeout, this, &AuthBridge::pollNow);
}

AuthBridge::~AuthBridge() = default;

void AuthBridge::resetPairing() {
    pollTimer_->stop();
    pairing_.reset();
    trust_.reset();
    pendingServerUrl_.clear();
}

void AuthBridge::restoreSession() {
    const auto session = repository_->load();
    if (!session) {
        core::Logger::info("Anmeldung: keine Sitzung abgelegt");
        emit noSession();
        return;
    }

    const QString serverUrl = QString::fromStdString(session->serverUrl);

    http_ = std::make_unique<core::HttpClient>(
        session->serverUrl,
        transport_,
        core::TlsPolicy::pinnedFromConfirmedCertificate(session->publicKeyPin));

    authenticated_ =
        std::make_unique<core::AuthenticatedClient>(*http_, *repository_, *session);

    const auto result = authenticated_->get("/api/auth/me");

    switch (result.outcome) {
        case core::AuthOutcome::Ok:
        case core::AuthOutcome::Refreshed:
            if (result.response.ok()) {
                core::Logger::info("Anmeldung: Sitzung wiederhergestellt für {}",
                                   session->username);
                emit sessionEstablished(serverUrl, QString::fromStdString(session->username));
                return;
            }
            if (result.response.statusCode == 401) {
                // Erneuert und trotzdem abgewiesen: die Bindung ist beim
                // Server nicht mehr gültig.
                emit reauthRequired(tr("Die Anmeldung gilt nicht mehr. Bitte neu verbinden."));
                return;
            }
            emit serverUnreachable(tr("Der Server antwortet unerwartet (Status %1).")
                                       .arg(result.response.statusCode));
            return;

        case core::AuthOutcome::ReauthRequired:
            emit reauthRequired(tr("Die Sitzung ist abgelaufen. Bitte neu verbinden."));
            return;

        case core::AuthOutcome::TrustBroken:
            emit certificateChanged(serverUrl,
                                    QString::fromStdString(result.response.detail));
            return;

        case core::AuthOutcome::Unreachable:
            emit serverUnreachable(tr("Der Server ist zurzeit nicht erreichbar."));
            return;
    }
}

void AuthBridge::logout() {
    const bool complete = repository_->clear();

    authenticated_.reset();
    http_.reset();
    resetPairing();

    if (complete) {
        emit loggedOut();
        return;
    }

    emit logoutIncomplete(
        tr("Die Sitzung wurde entfernt, aber nicht alles liess sich löschen. "
           "Bitte im Anmeldeinformationsspeicher unter „BaluDesk“ nachsehen."));
}

// --- Paaren: kommt in Task 6 -------------------------------------------

void AuthBridge::connectToServer(const QString& rawUrl) {
    Q_UNUSED(rawUrl);
}

void AuthBridge::confirmCertificate() {}

void AuthBridge::rejectCertificate() {}

void AuthBridge::cancelPairing() {}

void AuthBridge::pollNow() {}

void AuthBridge::beginPairing() {}

void AuthBridge::finishPairing() {}

}  // namespace baludesk::bridge
```

`nowSeconds()` ist in diesem Task noch ungenutzt und würde als Warnung auffallen. Damit der Build warnungsfrei bleibt, die Funktion erst in Task 6 einfügen — hier stattdessen weglassen und in Task 6 mit dem ersten Aufrufer nachziehen. Ebenso `#include <chrono>`, `#include <cstdint>` und `#include "core/api/ServerUrl.h"`.

- [ ] **Step 9: Tests laufen lassen und grün bestätigen**

```powershell
cmake --preset msvc2022
cmake --build --preset debug
ctest --preset debug
```

Erwartet: **164 Tests, alle bestanden** (157 plus 7 neue). Keine Warnungen aus eigenem Code.

- [ ] **Step 10: Committen**

```powershell
git add src/bridge tests/bridge tests/CMakeLists.txt CMakeLists.txt src/app/CMakeLists.txt
git commit -m @'
feat(bridge): AuthBridge stellt die Sitzung wieder her und meldet ab

Die einzige Stelle, an der beide Welten sich beruehren. Sie besitzt alles,
was blockiert, und erzeugt bewusst keinen Thread - wo sie lebt, entscheidet
ihr Eigentuemer. Dadurch ist sie ohne Ereignisschleife pruefbar.

baludesk_bridge linkt Qt6::Core, nicht Qt6::Widgets: ein Dialog im
Arbeiterthread wird damit zum Uebersetzungsfehler statt zum Laufzeitfehler.
'@
```

---

## Task 6: AuthBridge — Paaren

**Files:**
- Modify: `src/bridge/AuthBridge.cpp` (die sieben leeren Methoden aus Task 5 füllen)
- Test: `tests/bridge/auth_bridge_test.cpp` (ergänzen)

**Interfaces:**
- Consumes: `TrustNegotiation` (Task 1), `PairingClient`, `checkServerUrl`, `FileSessionRepository::save`, `AuthenticatedClient` (Task 3)
- Produces: keine neuen Typen — füllt die in Task 5 deklarierten Slots und Signale mit Verhalten

---

- [ ] **Step 1: Die fehlschlagenden Tests schreiben**

An `tests/bridge/auth_bridge_test.cpp` anhängen. Die Hilfsfunktionen aus Task 5 stehen bereits oben in der Datei; im anonymen Namensraum zusätzlich ergänzen:

```cpp
const char* kDeviceCodeResponse = R"({
  "device_code": "geheimer-geraetecode",
  "user_code": "483920",
  "verification_url": "https://nas.example.com/devices?pair=1",
  "expires_in": 600,
  "interval": 5
})";

const char* kApprovedResponse = R"({
  "status": "approved",
  "access_token": "zugangstoken",
  "refresh_token": "erneuerungstoken",
  "token_type": "bearer",
  "user": {"id": 1, "username": "sven", "email": "sven@example.com", "role": "admin"}
})";

// Ein echtes Zertifikat, damit identifyCertificate() etwas zu tun bekommt.
std::string testCertificate() {
    const std::filesystem::path path =
        std::filesystem::path(BALUDESK_TEST_DATA_DIR) / "test-cert-a.pem";
    std::ifstream stream(path, std::ios::binary);
    EXPECT_TRUE(stream.good()) << "Testdatei nicht lesbar: " << path.string();
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

TransportResult untrustedResult(std::string pem) {
    TransportResult result;
    result.error = HttpErrorKind::CertificateUntrusted;
    result.detail = "self signed certificate";
    result.peerCertificatePem = std::move(pem);
    return result;
}
```

Dazu oben `#include <filesystem>` und `#include <fstream>` ergänzen.

Und die Tests:

```cpp
TEST(AuthBridgeTest, RejectsABadServerUrlWithoutTouchingTheNetwork) {
    Fixture f;
    QSignalSpy rejected(f.bridge.get(), &AuthBridge::serverUrlRejected);

    f.bridge->connectToServer(QStringLiteral("http://nas.example.com"));

    // Über diese Verbindung ginge ein Zugangstoken. Die Prüfung liegt in
    // core::checkServerUrl und greift vor jedem Netzkontakt.
    ASSERT_EQ(rejected.count(), 1);
    EXPECT_FALSE(rejected.at(0).at(0).toString().isEmpty());
    EXPECT_TRUE(f.transport->calls.empty());
}

TEST(AuthBridgeTest, EmitsTheCodeWhenTheCertificateIsFine) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, R"({"status":"ok"})"));
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));

    QSignalSpy ready(f.bridge.get(), &AuthBridge::pairingCodeReady);
    f.bridge->connectToServer(QStringLiteral("nas.example.com"));

    ASSERT_EQ(ready.count(), 1);
    EXPECT_EQ(ready.at(0).at(0).toString(), QStringLiteral("483920"));
    EXPECT_EQ(ready.at(0).at(1).toString(),
              QStringLiteral("https://nas.example.com/devices?pair=1"));
    EXPECT_EQ(ready.at(0).at(2).toInt(), 600);

    ASSERT_EQ(f.transport->calls.size(), std::size_t{2});
    EXPECT_EQ(f.transport->calls[0].url, "https://nas.example.com/api/health");
    EXPECT_EQ(f.transport->calls[1].url,
              "https://nas.example.com/api/desktop-pairing/device-code");
    // Die Gerätekennung geht so mit, wie sie hereingereicht wurde.
    EXPECT_NE(f.transport->calls[1].request.jsonBody.find("geraet-1"), std::string::npos);
    EXPECT_NE(f.transport->calls[1].request.jsonBody.find("Svens Rechner"), std::string::npos);
}

TEST(AuthBridgeTest, AsksForCertificateConfirmationOnAnUnknownCertificate) {
    Fixture f;
    f.transport->queueResult(untrustedResult(testCertificate()));

    QSignalSpy confirmation(f.bridge.get(), &AuthBridge::certificateConfirmationRequired);
    QSignalSpy ready(f.bridge.get(), &AuthBridge::pairingCodeReady);
    f.bridge->connectToServer(QStringLiteral("nas.local"));

    ASSERT_EQ(confirmation.count(), 1);
    EXPECT_FALSE(confirmation.at(0).at(0).toString().isEmpty());
    EXPECT_EQ(confirmation.at(0).at(1).toString(), QStringLiteral("self signed certificate"));
    // Vor der Entscheidung wird nicht gepaart.
    EXPECT_EQ(ready.count(), 0);
    EXPECT_EQ(f.transport->calls.size(), std::size_t{1});
}

TEST(AuthBridgeTest, ConfirmingTheCertificatePinsItAndContinues) {
    Fixture f;
    f.transport->queueResult(untrustedResult(testCertificate()));
    f.bridge->connectToServer(QStringLiteral("nas.local"));

    f.transport->queueResult(jsonResult(200, R"({"status":"ok"})"));
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));

    QSignalSpy ready(f.bridge.get(), &AuthBridge::pairingCodeReady);
    f.bridge->confirmCertificate();

    EXPECT_EQ(ready.count(), 1);
    ASSERT_EQ(f.transport->calls.size(), std::size_t{3});
    // Ab der erneuten Prüfung trägt jeder Aufruf die angeheftete Regel.
    EXPECT_EQ(f.transport->calls[1].policy.mode,
              baludesk::core::TlsPolicy::Mode::Pinned);
    EXPECT_EQ(f.transport->calls[2].policy.mode,
              baludesk::core::TlsPolicy::Mode::Pinned);
}

TEST(AuthBridgeTest, RejectingTheCertificateEndsTheAttempt) {
    Fixture f;
    f.transport->queueResult(untrustedResult(testCertificate()));
    f.bridge->connectToServer(QStringLiteral("nas.local"));
    const std::size_t callsBefore = f.transport->calls.size();

    QSignalSpy cancelled(f.bridge.get(), &AuthBridge::pairingCancelled);
    f.bridge->rejectCertificate();

    EXPECT_EQ(cancelled.count(), 1);
    // Nichts angeheftet, nichts gesendet.
    EXPECT_EQ(f.transport->calls.size(), callsBefore);
}

TEST(AuthBridgeTest, StoresTheSessionOnApproval) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, R"({"status":"ok"})"));
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.bridge->connectToServer(QStringLiteral("nas.example.com"));

    f.transport->queueResult(jsonResult(200, kApprovedResponse));

    QSignalSpy established(f.bridge.get(), &AuthBridge::sessionEstablished);
    f.bridge->pollNow();

    ASSERT_EQ(established.count(), 1);
    EXPECT_EQ(established.at(0).at(0).toString(), QStringLiteral("https://nas.example.com"));
    EXPECT_EQ(established.at(0).at(1).toString(), QStringLiteral("sven"));

    // Die Token liegen nachweislich in der Ablage - der Server liefert sie
    // kein zweites Mal aus.
    EXPECT_EQ(f.secrets->entries["session/access-token"], "zugangstoken");
    EXPECT_EQ(f.secrets->entries["session/refresh-token"], "erneuerungstoken");
    EXPECT_TRUE(QFile::exists(f.directory.filePath(QStringLiteral("session.json"))));
}

TEST(AuthBridgeTest, StoresTheConfirmedPinWithTheSession) {
    Fixture f;
    f.transport->queueResult(untrustedResult(testCertificate()));
    f.bridge->connectToServer(QStringLiteral("nas.local"));
    f.transport->queueResult(jsonResult(200, R"({"status":"ok"})"));
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.bridge->confirmCertificate();

    f.transport->queueResult(jsonResult(200, kApprovedResponse));
    f.bridge->pollNow();

    // Ohne den Pin in der Sitzung verbindet sich der Client beim nächsten
    // Start nicht mehr - die Bestätigung wäre für einen Programmlauf gewesen.
    baludesk::core::FileSessionRepository repository(f.sessionPath(), *f.secrets);
    const auto stored = repository.load();
    ASSERT_TRUE(stored.has_value());
    EXPECT_FALSE(stored->publicKeyPin.empty());
}

TEST(AuthBridgeTest, ReportsAFailureWhenTheSessionCannotBeStored) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, R"({"status":"ok"})"));
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.bridge->connectToServer(QStringLiteral("nas.example.com"));

    f.transport->queueResult(jsonResult(200, kApprovedResponse));
    f.secrets->failOnSaveNumber = 1;

    QSignalSpy failed(f.bridge.get(), &AuthBridge::pairingFailed);
    QSignalSpy established(f.bridge.get(), &AuthBridge::sessionEstablished);
    f.bridge->pollNow();

    // Der Server hat seinen Datensatz beim Ausliefern gelöscht. Was hier
    // verlorengeht, ist endgültig verloren - das gehört gesagt, nicht
    // verschwiegen.
    EXPECT_EQ(established.count(), 0);
    ASSERT_EQ(failed.count(), 1);
    EXPECT_FALSE(failed.at(0).at(0).toString().isEmpty());
}

TEST(AuthBridgeTest, CancelDuringPollingStopsTheAttempt) {
    Fixture f;
    f.transport->queueResult(jsonResult(200, R"({"status":"ok"})"));
    f.transport->queueResult(jsonResult(200, kDeviceCodeResponse));
    f.bridge->connectToServer(QStringLiteral("nas.example.com"));
    const std::size_t callsBefore = f.transport->calls.size();

    QSignalSpy cancelled(f.bridge.get(), &AuthBridge::pairingCancelled);
    f.bridge->cancelPairing();

    EXPECT_EQ(cancelled.count(), 1);

    // Nach dem Abbruch fragt auch ein Abfrageschritt nicht mehr nach.
    f.bridge->pollNow();
    EXPECT_EQ(f.transport->calls.size(), callsBefore);
}
```

- [ ] **Step 2: Tests laufen lassen und Fehlschlag bestätigen**

```powershell
cmake --build --preset debug
ctest --preset debug
```

Erwartet: die neun neuen Tests scheitern (die Slots aus Task 5 tun nichts), die 164 vorhandenen bleiben grün.

- [ ] **Step 3: Die Include- und Hilfsfunktionsergänzung in `AuthBridge.cpp` nachziehen**

Oben in `src/bridge/AuthBridge.cpp` ergänzen:

```cpp
#include <chrono>
#include <cstdint>

#include "core/api/ServerUrl.h"
```

Und im anonymen Namensraum:

```cpp
std::int64_t nowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
```

- [ ] **Step 4: Die sieben leeren Methoden ersetzen**

Den Block ab `// --- Paaren: kommt in Task 6 ---` bis zum schliessenden Namensraum durch dies ersetzen:

```cpp
void AuthBridge::connectToServer(const QString& rawUrl) {
    resetPairing();

    // Die Regel gilt für jeden Aufrufer, nicht nur für den Dialog - deshalb
    // liegt sie in core und wird hier nur benutzt.
    const auto check = core::checkServerUrl(rawUrl.toStdString());
    if (!check.ok()) {
        emit serverUrlRejected(QString::fromStdString(check.message));
        return;
    }

    pendingServerUrl_ = QString::fromStdString(check.normalized);

    // Frische Verbindung mit voller Prüfung. Ein etwa früher angehefteter
    // Schlüssel gehört zu einer anderen Bindung und hätte hier nichts zu
    // suchen.
    http_ = std::make_unique<core::HttpClient>(check.normalized, transport_, core::TlsPolicy{});
    trust_ = std::make_unique<core::TrustNegotiation>(*http_);

    switch (trust_->probe()) {
        case core::TrustState::Trusted:
            beginPairing();
            return;

        case core::TrustState::ConfirmationRequired:
            emit certificateConfirmationRequired(
                QString::fromStdString(trust_->prompt()->fingerprintSha256),
                QString::fromStdString(trust_->prompt()->reason));
            return;

        case core::TrustState::PinMismatch:
            emit certificateChanged(pendingServerUrl_,
                                    QString::fromStdString(trust_->message()));
            return;

        case core::TrustState::Unreachable:
        case core::TrustState::Failed:
        case core::TrustState::Idle:
            emit pairingFailed(QString::fromStdString(trust_->message()));
            resetPairing();
            return;
    }
}

void AuthBridge::confirmCertificate() {
    if (!trust_) {
        return;
    }

    if (trust_->confirm() == core::TrustState::Trusted) {
        beginPairing();
        return;
    }

    emit pairingFailed(QString::fromStdString(trust_->message()));
    resetPairing();
}

void AuthBridge::rejectCertificate() {
    resetPairing();
    emit pairingCancelled();
}

void AuthBridge::cancelPairing() {
    resetPairing();
    emit pairingCancelled();
}

void AuthBridge::beginPairing() {
    pairing_ = std::make_unique<core::PairingClient>(*http_);

    const auto progress = pairing_->begin(deviceId_.toStdString(),
                                          deviceName_.toStdString(),
                                          nowSeconds());

    if (progress.state != core::PairingState::WaitingForApproval) {
        emit pairingFailed(QString::fromStdString(progress.message));
        resetPairing();
        return;
    }

    emit pairingCodeReady(QString::fromStdString(pairing_->prompt()->userCode),
                          QString::fromStdString(pairing_->prompt()->verificationUrl),
                          pairing_->prompt()->expiresInSeconds);

    // Das vom Server vorgegebene Intervall ist einzuhalten und nicht zu
    // unterbieten: bei 12 zugelassenen Abrufen je Minute und 5 Sekunden
    // Vorgabe gibt es keine Reserve.
    pollTimer_->start(progress.waitSeconds * 1000);
}

void AuthBridge::pollNow() {
    if (!pairing_) {
        return;
    }

    const auto progress = pairing_->poll(nowSeconds());

    if (progress.state == core::PairingState::WaitingForApproval) {
        if (!progress.message.empty()) {
            emit pairingNotice(QString::fromStdString(progress.message));
        }
        pollTimer_->start(progress.waitSeconds * 1000);
        return;
    }

    if (progress.state == core::PairingState::Approved) {
        finishPairing();
        return;
    }

    emit pairingFailed(QString::fromStdString(progress.message));
    resetPairing();
}

void AuthBridge::finishPairing() {
    // Zwischen Approved und save() darf nichts stehen, was scheitern kann:
    // der Server hat seinen Datensatz beim Ausliefern gelöscht und antwortet
    // danach mit 404. Was hier verlorengeht, ist endgültig verloren.
    core::Session session;
    session.serverUrl = pendingServerUrl_.toStdString();
    session.username = pairing_->tokens()->username;
    session.publicKeyPin = trust_->confirmedPin();
    session.accessToken = pairing_->tokens()->accessToken;
    session.refreshToken = pairing_->tokens()->refreshToken;

    if (!repository_->save(session)) {
        core::Logger::error("Paarung: Sitzung konnte nicht abgelegt werden");
        emit pairingFailed(
            tr("Die Zugangsdaten konnten nicht abgelegt werden. Der Server liefert sie "
               "kein zweites Mal — bitte erneut verbinden."));
        resetPairing();
        return;
    }

    const QString username = QString::fromStdString(session.username);
    const QString serverUrl = pendingServerUrl_;

    authenticated_ = std::make_unique<core::AuthenticatedClient>(*http_, *repository_, session);

    core::Logger::info("Paarung: abgeschlossen für Benutzer {}", session.username);
    resetPairing();
    emit sessionEstablished(serverUrl, username);
}

}  // namespace baludesk::bridge
```

*Hinweis:* `resetPairing()` löscht `pendingServerUrl_`, deshalb werden `serverUrl` und `username` **vor** dem Aufruf in lokale Variablen gezogen. Wer das umdreht, meldet eine leere Adresse an die Oberfläche.

- [ ] **Step 5: Tests laufen lassen und grün bestätigen**

```powershell
cmake --build --preset debug
ctest --preset debug
```

Erwartet: **173 Tests, alle bestanden** (164 plus 9 neue).

- [ ] **Step 6: Committen**

```powershell
git add src/bridge/AuthBridge.cpp tests/bridge/auth_bridge_test.cpp
git commit -m @'
feat(bridge): AuthBridge fuehrt den Paarungsablauf

Adresse pruefen, Vertrauen klaeren, Code holen, am Zeitgeber abfragen,
Sitzung ablegen. Zwischen Approved und save() steht nichts, was scheitern
kann - der Server liefert die Token genau einmal aus.

Der bestaetigte Pin wandert mit in die Sitzung; ohne ihn waere die
Bestaetigung fuer einen einzigen Programmlauf gewesen.
'@
```

---

## Task 7: Die fünf Seiten

Reine Widgets: sie zeigen an, was man ihnen gibt, und melden, was jemand angeklickt hat. Keine Seite kennt `HttpClient`, `AuthBridge` oder einen Thread. Wer sie verdrahtet, ist Task 8.

Spec Abschnitt 4 der übergeordneten Spec schliesst automatisierte Oberflächentests aus. Eine Regel steckt hier trotzdem, die es verdient, geprüft zu werden: die Restlaufzeit-Anzeige. Sie bekommt deshalb eine eigene, freistehende Funktion.

**Files:**
- Create: `src/app/pairing/Countdown.h`, `src/app/pairing/Countdown.cpp`
- Create: `src/app/pairing/ServerUrlPage.h`, `.cpp`
- Create: `src/app/pairing/CertificatePage.h`, `.cpp`
- Create: `src/app/pairing/PairingCodePage.h`, `.cpp`
- Create: `src/app/pairing/CertificateChangedPage.h`, `.cpp`
- Create: `src/app/ConnectedPage.h`, `.cpp`
- Modify: `src/app/CMakeLists.txt`
- Test: `tests/app/countdown_test.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: nichts aus früheren Tasks — die Seiten sind für sich allein übersetzbar
- Produces:
  - `QString baludesk::app::formatRemaining(int seconds)`
  - `class ServerUrlPage : QWidget` — Signal `connectRequested(QString)`; Slots `showProblem(QString)`, `showNotice(QString)`, `setBusy(bool)`, `clearMessages()`
  - `class CertificatePage : QWidget` — Signale `confirmed()`, `rejected()`; Slot `show(QString fingerprint, QString reason)`
  - `class PairingCodePage : QWidget` — Signal `cancelled()`; Slots `showCode(QString code, QString url, int expiresInSeconds)`, `showNotice(QString)`, `tick()`
  - `class CertificateChangedPage : QWidget` — Signale `repairRequested()`, `quitRequested()`; Slot `show(QString serverUrl, QString reason)`
  - `class ConnectedPage : QWidget` — Signal `logoutRequested()`; Slots `show(QString serverUrl, QString username)`, `showOffline(QString)`, `clearOffline()`

---

- [ ] **Step 1: Den fehlschlagenden Test für die Restlaufzeit schreiben**

Datei `tests/app/countdown_test.cpp`:

```cpp
#include "app/pairing/Countdown.h"

#include <gtest/gtest.h>

#include <QString>

using baludesk::app::formatRemaining;

TEST(CountdownTest, FormatsMinutesAndSecondsWithATwoDigitSecond) {
    EXPECT_EQ(formatRemaining(600), QStringLiteral("10:00"));
    EXPECT_EQ(formatRemaining(581), QStringLiteral("9:41"));
    EXPECT_EQ(formatRemaining(65), QStringLiteral("1:05"));
    // Ohne die führende Null läse sich 1:05 als 1:5.
    EXPECT_EQ(formatRemaining(9), QStringLiteral("0:09"));
}

TEST(CountdownTest, NeverShowsANegativeTime) {
    // Der Zeitgeber der Oberfläche und die Gültigkeit des Codes laufen
    // unabhängig voneinander. Läuft die Anzeige über das Ende hinaus, soll
    // sie stehenbleiben statt "-0:03" zu zeigen.
    EXPECT_EQ(formatRemaining(0), QStringLiteral("0:00"));
    EXPECT_EQ(formatRemaining(-3), QStringLiteral("0:00"));
}

TEST(CountdownTest, KeepsCountingBeyondAnHour) {
    // Der Server gibt heute 600 Sekunden vor. Ein grösserer Wert darf die
    // Anzeige nicht in die Irre führen.
    EXPECT_EQ(formatRemaining(3661), QStringLiteral("61:01"));
}
```

- [ ] **Step 2: `tests/CMakeLists.txt` erweitern**

```cmake
add_executable(baludesk_app_tests
    app/settings_store_test.cpp
    app/qt_log_bridge_test.cpp
    app/countdown_test.cpp
    bridge/auth_bridge_test.cpp
)
```

- [ ] **Step 3: Test laufen lassen und Fehlschlag bestätigen**

```powershell
cmake --preset msvc2022
cmake --build --preset debug
```

Erwartet: FEHLER beim Übersetzen — `Cannot open include file: 'app/pairing/Countdown.h'`.

- [ ] **Step 4: `src/app/pairing/Countdown.h` und `.cpp` schreiben**

`src/app/pairing/Countdown.h`:

```cpp
#pragma once

#include <QString>

namespace baludesk::app {

// Restlaufzeit als „9:41". Negative Werte ergeben „0:00".
//
// Freistehend statt als Methode der Seite, weil das die einzige Regel der
// Oberfläche ist, die falsch sein kann, ohne dass es auffällt - und weil sie
// so ohne Widget prüfbar bleibt.
[[nodiscard]] QString formatRemaining(int seconds);

}  // namespace baludesk::app
```

`src/app/pairing/Countdown.cpp`:

```cpp
#include "app/pairing/Countdown.h"

namespace baludesk::app {

QString formatRemaining(int seconds) {
    const int clamped = seconds > 0 ? seconds : 0;
    const int minutes = clamped / 60;
    const int remainder = clamped % 60;

    // Die Sekunden zweistellig, die Minuten nicht: „9:41", nicht „09:41".
    return QStringLiteral("%1:%2").arg(minutes).arg(remainder, 2, 10, QLatin1Char('0'));
}

}  // namespace baludesk::app
```

- [ ] **Step 5: `src/app/pairing/ServerUrlPage.h` und `.cpp` schreiben**

`ServerUrlPage.h`:

```cpp
#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

namespace baludesk::app {

// Erste Seite: die Serveradresse eingeben.
//
// Prüft nichts selbst. Die Regel, welche Adresse zulässig ist, liegt in
// core::checkServerUrl und gilt für jeden Aufrufer - hier nachgebaut wäre sie
// eine zweite Wahrheit.
class ServerUrlPage : public QWidget {
    Q_OBJECT

public:
    explicit ServerUrlPage(QWidget* parent = nullptr);

public slots:
    // Meldung unter dem Eingabefeld, rot. Die Eingabe bleibt stehen.
    void showProblem(const QString& message);

    // Meldung unter dem Eingabefeld, unauffällig - etwa „Sitzung abgelaufen".
    void showNotice(const QString& message);

    void clearMessages();

    // Während ein Aufruf läuft. Das Feld bleibt lesbar, der Knopf wird
    // gesperrt - ein zweiter Klick würde einen zweiten Vorgang starten.
    void setBusy(bool busy);

signals:
    void connectRequested(const QString& rawUrl);

private:
    void requestConnect();

    QLineEdit* input_ = nullptr;
    QPushButton* connectButton_ = nullptr;
    QLabel* message_ = nullptr;
};

}  // namespace baludesk::app
```

`ServerUrlPage.cpp`:

```cpp
#include "app/pairing/ServerUrlPage.h"

#include <QFont>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace baludesk::app {

ServerUrlPage::ServerUrlPage(QWidget* parent) : QWidget(parent) {
    auto* heading = new QLabel(tr("Mit einem Server verbinden"), this);
    QFont headingFont = heading->font();
    headingFont.setPointSize(headingFont.pointSize() + 6);
    heading->setFont(headingFont);

    auto* label = new QLabel(tr("Serveradresse"), this);

    input_ = new QLineEdit(this);
    input_->setPlaceholderText(QStringLiteral("nas.example.com"));
    input_->setClearButtonEnabled(true);

    message_ = new QLabel(this);
    message_->setWordWrap(true);
    message_->setVisible(false);

    connectButton_ = new QPushButton(tr("Verbinden"), this);
    connectButton_->setDefault(true);

    auto* layout = new QVBoxLayout(this);
    layout->addStretch(1);
    layout->addWidget(heading);
    layout->addSpacing(12);
    layout->addWidget(label);
    layout->addWidget(input_);
    layout->addWidget(message_);
    layout->addSpacing(8);
    layout->addWidget(connectButton_, 0, Qt::AlignRight);
    layout->addStretch(2);

    connect(connectButton_, &QPushButton::clicked, this, &ServerUrlPage::requestConnect);
    // Eingabetaste im Feld tut dasselbe wie der Knopf.
    connect(input_, &QLineEdit::returnPressed, this, &ServerUrlPage::requestConnect);
}

void ServerUrlPage::requestConnect() {
    clearMessages();
    emit connectRequested(input_->text());
}

void ServerUrlPage::showProblem(const QString& message) {
    setBusy(false);
    message_->setText(message);
    message_->setStyleSheet(QStringLiteral("color: palette(bright-text);"));
    message_->setVisible(true);
    input_->setFocus();
}

void ServerUrlPage::showNotice(const QString& message) {
    setBusy(false);
    message_->setText(message);
    message_->setStyleSheet(QString());
    message_->setVisible(true);
}

void ServerUrlPage::clearMessages() {
    message_->clear();
    message_->setVisible(false);
}

void ServerUrlPage::setBusy(bool busy) {
    connectButton_->setEnabled(!busy);
    connectButton_->setText(busy ? tr("Verbinde…") : tr("Verbinden"));
}

}  // namespace baludesk::app
```

- [ ] **Step 6: `src/app/pairing/CertificatePage.h` und `.cpp` schreiben**

`CertificatePage.h`:

```cpp
#pragma once

#include <QWidget>

class QLabel;

namespace baludesk::app {

// Zweite Seite, nur bei Bedarf: den Fingerabdruck vergleichen lassen.
//
// Der Fingerabdruck steht in derselben Darstellung, die auch Browser und die
// openssl-Kommandozeile ausgeben - sonst wäre er nicht zu vergleichen, und
// genau das Vergleichen ist der ganze Sinn dieser Seite.
class CertificatePage : public QWidget {
    Q_OBJECT

public:
    explicit CertificatePage(QWidget* parent = nullptr);

public slots:
    void showCertificate(const QString& fingerprint, const QString& reason);

signals:
    void confirmed();
    void rejected();

private:
    QLabel* reason_ = nullptr;
    QLabel* fingerprint_ = nullptr;
};

}  // namespace baludesk::app
```

`CertificatePage.cpp`:

```cpp
#include "app/pairing/CertificatePage.h"

#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace baludesk::app {

CertificatePage::CertificatePage(QWidget* parent) : QWidget(parent) {
    auto* heading = new QLabel(tr("Zertifikat des Servers bestätigen"), this);
    QFont headingFont = heading->font();
    headingFont.setPointSize(headingFont.pointSize() + 6);
    heading->setFont(headingFont);

    auto* explanation = new QLabel(
        tr("Dem Zertifikat dieses Servers wird nicht automatisch vertraut. "
           "Vergleiche den Fingerabdruck mit dem, den der Server anzeigt. "
           "Stimmen beide überein, ist die Verbindung sicher."),
        this);
    explanation->setWordWrap(true);

    reason_ = new QLabel(this);
    reason_->setWordWrap(true);

    fingerprint_ = new QLabel(this);
    fingerprint_->setWordWrap(true);
    // Feste Zeichenbreite: nur so lassen sich zwei Hexfolgen sinnvoll
    // nebeneinanderhalten.
    fingerprint_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    // Zum Kopieren, damit der Vergleich nicht abgetippt werden muss.
    fingerprint_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* reject = new QPushButton(tr("Abbrechen"), this);
    auto* confirm = new QPushButton(tr("Vertrauen und fortfahren"), this);
    confirm->setDefault(true);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    buttons->addWidget(reject);
    buttons->addWidget(confirm);

    auto* layout = new QVBoxLayout(this);
    layout->addStretch(1);
    layout->addWidget(heading);
    layout->addSpacing(12);
    layout->addWidget(explanation);
    layout->addSpacing(8);
    layout->addWidget(reason_);
    layout->addSpacing(8);
    layout->addWidget(fingerprint_);
    layout->addSpacing(16);
    layout->addLayout(buttons);
    layout->addStretch(2);

    connect(confirm, &QPushButton::clicked, this, &CertificatePage::confirmed);
    connect(reject, &QPushButton::clicked, this, &CertificatePage::rejected);
}

void CertificatePage::showCertificate(const QString& fingerprint, const QString& reason) {
    reason_->setText(tr("Grund: %1").arg(reason));
    fingerprint_->setText(fingerprint);
}

}  // namespace baludesk::app
```

- [ ] **Step 7: `src/app/pairing/PairingCodePage.h` und `.cpp` schreiben**

`PairingCodePage.h`:

```cpp
#pragma once

#include <QWidget>

class QLabel;
class QTimer;

namespace baludesk::app {

// Dritte Seite: den Code zeigen und auf die Freigabe warten.
//
// Der Zeitgeber hier zählt nur die Anzeige herunter und fasst nichts an, was
// blockiert. Die eigentliche Abfrage läuft in der AuthBridge, in deren
// Thread.
class PairingCodePage : public QWidget {
    Q_OBJECT

public:
    explicit PairingCodePage(QWidget* parent = nullptr);

public slots:
    void showCode(const QString& userCode, const QString& verificationUrl, int expiresInSeconds);
    void showNotice(const QString& message);

    // Hält den Countdown an, etwa beim Verlassen der Seite.
    void stop();

signals:
    void cancelled();

private:
    void updateRemaining();

    QLabel* code_ = nullptr;
    QLabel* url_ = nullptr;
    QLabel* remaining_ = nullptr;
    QLabel* notice_ = nullptr;
    QTimer* tick_ = nullptr;
    QString verificationUrl_;
    int remainingSeconds_ = 0;
};

}  // namespace baludesk::app
```

`PairingCodePage.cpp`:

```cpp
#include "app/pairing/PairingCodePage.h"

#include <QDesktopServices>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include "app/pairing/Countdown.h"

namespace baludesk::app {

PairingCodePage::PairingCodePage(QWidget* parent) : QWidget(parent) {
    auto* heading = new QLabel(tr("Code im Browser eingeben"), this);
    QFont headingFont = heading->font();
    headingFont.setPointSize(headingFont.pointSize() + 6);
    heading->setFont(headingFont);

    code_ = new QLabel(this);
    QFont codeFont = code_->font();
    codeFont.setPointSize(codeFont.pointSize() + 22);
    codeFont.setBold(true);
    // Weit gesperrt, damit sich die sechs Ziffern einzeln ablesen lassen.
    codeFont.setLetterSpacing(QFont::AbsoluteSpacing, 8);
    code_->setFont(codeFont);
    code_->setAlignment(Qt::AlignCenter);
    code_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    url_ = new QLabel(this);
    url_->setWordWrap(true);
    url_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* openBrowser = new QPushButton(tr("Im Browser öffnen"), this);

    remaining_ = new QLabel(this);
    notice_ = new QLabel(this);
    notice_->setWordWrap(true);
    notice_->setVisible(false);

    auto* cancel = new QPushButton(tr("Abbrechen"), this);

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(openBrowser);
    buttons->addStretch(1);
    buttons->addWidget(cancel);

    auto* layout = new QVBoxLayout(this);
    layout->addStretch(1);
    layout->addWidget(heading);
    layout->addSpacing(16);
    layout->addWidget(code_);
    layout->addSpacing(16);
    layout->addWidget(url_);
    layout->addWidget(remaining_);
    layout->addWidget(notice_);
    layout->addSpacing(16);
    layout->addLayout(buttons);
    layout->addStretch(2);

    tick_ = new QTimer(this);
    tick_->setInterval(1000);
    connect(tick_, &QTimer::timeout, this, &PairingCodePage::updateRemaining);

    connect(cancel, &QPushButton::clicked, this, [this]() {
        stop();
        emit cancelled();
    });
    connect(openBrowser, &QPushButton::clicked, this, [this]() {
        if (!verificationUrl_.isEmpty()) {
            QDesktopServices::openUrl(QUrl(verificationUrl_));
        }
    });
}

void PairingCodePage::showCode(const QString& userCode,
                               const QString& verificationUrl,
                               int expiresInSeconds) {
    code_->setText(userCode);
    verificationUrl_ = verificationUrl;
    url_->setText(tr("Öffne %1, melde dich an und gib den Code ein.").arg(verificationUrl));
    notice_->clear();
    notice_->setVisible(false);

    remainingSeconds_ = expiresInSeconds;
    updateRemaining();
    tick_->start();
}

void PairingCodePage::showNotice(const QString& message) {
    notice_->setText(message);
    notice_->setVisible(!message.isEmpty());
}

void PairingCodePage::stop() {
    tick_->stop();
}

void PairingCodePage::updateRemaining() {
    remaining_->setText(tr("Warte auf Freigabe…  (noch %1)").arg(formatRemaining(remainingSeconds_)));
    if (remainingSeconds_ <= 0) {
        // Die Bridge meldet das Ablaufen selbst; hier bleibt die Anzeige
        // einfach stehen, statt ins Negative zu laufen.
        tick_->stop();
        return;
    }
    --remainingSeconds_;
}

}  // namespace baludesk::app
```

- [ ] **Step 8: `src/app/pairing/CertificateChangedPage.h` und `.cpp` schreiben**

`CertificateChangedPage.h`:

```cpp
#pragma once

#include <QWidget>

class QLabel;

namespace baludesk::app {

// Sackgasse mit genau zwei Auswegen.
//
// Spec Abschnitt 2.9 verlangt bei einer Zertifikatsabweichung den harten
// Abbruch und verwirft eine Rückfrage „trotzdem fortfahren?" ausdrücklich -
// sie würde jeden MITM durchwinken. Diese Seite nimmt der Regel nichts von
// ihrer Härte, sondern nur ihre Sprachlosigkeit.
class CertificateChangedPage : public QWidget {
    Q_OBJECT

public:
    explicit CertificateChangedPage(QWidget* parent = nullptr);

public slots:
    void showChange(const QString& serverUrl, const QString& reason);

signals:
    // Abmelden und neu paaren - der einzige Weg zurück.
    void repairRequested();
    void quitRequested();

private:
    QLabel* explanation_ = nullptr;
    QLabel* reason_ = nullptr;
};

}  // namespace baludesk::app
```

`CertificateChangedPage.cpp`:

```cpp
#include "app/pairing/CertificateChangedPage.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace baludesk::app {

CertificateChangedPage::CertificateChangedPage(QWidget* parent) : QWidget(parent) {
    auto* heading = new QLabel(tr("⚠  Das Zertifikat hat sich geändert"), this);
    QFont headingFont = heading->font();
    headingFont.setPointSize(headingFont.pointSize() + 6);
    heading->setFont(headingFont);

    explanation_ = new QLabel(this);
    explanation_->setWordWrap(true);

    reason_ = new QLabel(this);
    reason_->setWordWrap(true);

    auto* consequence = new QLabel(
        tr("Das ist normal, wenn der Server ein neues Zertifikat bekommen hat. Es kann "
           "aber auch bedeuten, dass sich jemand dazwischenschaltet.\n\n"
           "Die Verbindung bleibt getrennt, bis du dich neu verbindest. Vergewissere "
           "dich vorher, dass der Server tatsächlich ein neues Zertifikat hat."),
        this);
    consequence->setWordWrap(true);

    auto* quit = new QPushButton(tr("Beenden"), this);
    auto* repair = new QPushButton(tr("Abmelden und neu verbinden"), this);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    buttons->addWidget(quit);
    buttons->addWidget(repair);

    auto* layout = new QVBoxLayout(this);
    layout->addStretch(1);
    layout->addWidget(heading);
    layout->addSpacing(12);
    layout->addWidget(explanation_);
    layout->addSpacing(8);
    layout->addWidget(reason_);
    layout->addSpacing(8);
    layout->addWidget(consequence);
    layout->addSpacing(16);
    layout->addLayout(buttons);
    layout->addStretch(2);

    connect(repair, &QPushButton::clicked, this, &CertificateChangedPage::repairRequested);
    connect(quit, &QPushButton::clicked, this, &CertificateChangedPage::quitRequested);
}

void CertificateChangedPage::showChange(const QString& serverUrl, const QString& reason) {
    explanation_->setText(
        tr("Der Server %1 weist ein anderes Zertifikat vor als das, das beim Verbinden "
           "bestätigt wurde.")
            .arg(serverUrl));
    reason_->setText(reason.isEmpty() ? QString() : tr("Meldung: %1").arg(reason));
    reason_->setVisible(!reason.isEmpty());
}

}  // namespace baludesk::app
```

- [ ] **Step 9: `src/app/ConnectedPage.h` und `.cpp` schreiben**

`ConnectedPage.h`:

```cpp
#pragma once

#include <QWidget>

class QLabel;

namespace baludesk::app {

// Der verbundene Zustand. In Stufe 1c zeigt sie, mit wem man verbunden ist,
// und bietet das Abmelden an; ab Stufe 2 kommt der Sync-Zustand hierher.
class ConnectedPage : public QWidget {
    Q_OBJECT

public:
    explicit ConnectedPage(QWidget* parent = nullptr);

public slots:
    void showConnection(const QString& serverUrl, const QString& username);

    // Ein schlafender Server ist laut Spec Abschnitt 10 ein normaler
    // Betriebszustand, kein Fehler - die Sitzung bleibt bestehen.
    void showOffline(const QString& message);
    void clearOffline();

signals:
    void logoutRequested();

private:
    QLabel* connection_ = nullptr;
    QLabel* offline_ = nullptr;
};

}  // namespace baludesk::app
```

`ConnectedPage.cpp`:

```cpp
#include "app/ConnectedPage.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace baludesk::app {

ConnectedPage::ConnectedPage(QWidget* parent) : QWidget(parent) {
    connection_ = new QLabel(this);
    QFont connectionFont = connection_->font();
    connectionFont.setPointSize(connectionFont.pointSize() + 4);
    connection_->setFont(connectionFont);
    connection_->setWordWrap(true);

    offline_ = new QLabel(this);
    offline_->setWordWrap(true);
    offline_->setVisible(false);

    auto* outlook = new QLabel(
        tr("Die Synchronisation folgt in der nächsten Ausbaustufe."), this);
    outlook->setWordWrap(true);

    auto* logout = new QPushButton(tr("Abmelden"), this);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    buttons->addWidget(logout);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(connection_);
    layout->addWidget(offline_);
    layout->addSpacing(16);
    layout->addWidget(outlook);
    layout->addStretch(1);
    layout->addLayout(buttons);

    connect(logout, &QPushButton::clicked, this, &ConnectedPage::logoutRequested);
}

void ConnectedPage::showConnection(const QString& serverUrl, const QString& username) {
    connection_->setText(tr("Verbunden mit %1 als %2").arg(serverUrl, username));
}

void ConnectedPage::showOffline(const QString& message) {
    offline_->setText(message);
    offline_->setVisible(true);
}

void ConnectedPage::clearOffline() {
    offline_->clear();
    offline_->setVisible(false);
}

}  // namespace baludesk::app
```

- [ ] **Step 10: `src/app/CMakeLists.txt` erweitern**

Die Quelldateiliste von `baludesk_app_lib` ersetzen, alles Übrige unverändert lassen:

```cmake
add_library(baludesk_app_lib STATIC
    MainWindow.cpp
    SettingsStore.cpp
    QtLogBridge.cpp
    ConnectedPage.cpp
    pairing/Countdown.cpp
    pairing/ServerUrlPage.cpp
    pairing/CertificatePage.cpp
    pairing/PairingCodePage.cpp
    pairing/CertificateChangedPage.cpp
)
```

- [ ] **Step 11: Tests laufen lassen und grün bestätigen**

```powershell
cmake --preset msvc2022
cmake --build --preset debug
ctest --preset debug
```

Erwartet: **176 Tests, alle bestanden** (173 plus 3 neue). Keine Warnungen aus eigenem Code.

- [ ] **Step 12: Committen**

```powershell
git add src/app/pairing src/app/ConnectedPage.h src/app/ConnectedPage.cpp src/app/CMakeLists.txt tests/app/countdown_test.cpp tests/CMakeLists.txt
git commit -m @'
feat(app): die fuenf Seiten der Anmeldung

Reine Widgets: sie zeigen an, was man ihnen gibt, und melden Klicks. Keine
kennt HttpClient, AuthBridge oder einen Thread.

Die Restlaufzeit-Anzeige liegt als freistehende Funktion daneben - die
einzige Regel dieser Schicht, die falsch sein kann, ohne aufzufallen.
'@
```

---

## Task 8: MainWindow — Stapel, Thread, Verdrahtung

Hier fällt die Threading-Entscheidung, und zwar an genau einer Stelle. `MainWindow` besitzt den `QThread` und die `AuthBridge`; alles dazwischen sind Signale.

**Files:**
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`
- Modify: `src/app/main.cpp`

**Interfaces:**
- Consumes: `AuthBridge` samt allen Slots und Signalen (Tasks 5 und 6), die fünf Seiten (Task 7), `SettingsStore::deviceId`/`deviceName` (Task 4), `core::defaultSessionPath`, `core::CurlTransport`, `core::WindowsCredentialStore`
- Produces: keine Typen für spätere Tasks — dies ist die Spitze

---

- [ ] **Step 1: `src/app/MainWindow.h` ersetzen**

```cpp
#pragma once

#include <QMainWindow>

class QStackedWidget;
class QThread;

namespace baludesk::bridge {
class AuthBridge;
}

namespace baludesk::app {

class CertificateChangedPage;
class CertificatePage;
class ConnectedPage;
class PairingCodePage;
class ServerUrlPage;

// Hauptfenster: ein Stapel von Ansichten und der Eigentümer des
// Arbeiterthreads.
//
// DIES IST DIE EINZIGE STELLE, AN DER EIN THREAD ENTSTEHT. Die AuthBridge
// selbst weiss nichts davon; sie wird hier per moveToThread verschoben, und
// alle Verbindungen laufen von da an über die Ereigniswarteschlange. Wer
// Nebenläufigkeitsfehler sucht, sucht hier.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void buildPages();
    void startAuthThread();
    void wireBridge();

    QStackedWidget* stack_ = nullptr;

    ServerUrlPage* serverUrlPage_ = nullptr;
    CertificatePage* certificatePage_ = nullptr;
    PairingCodePage* pairingCodePage_ = nullptr;
    CertificateChangedPage* certificateChangedPage_ = nullptr;
    ConnectedPage* connectedPage_ = nullptr;

    QThread* authThread_ = nullptr;
    bridge::AuthBridge* bridge_ = nullptr;
};

}  // namespace baludesk::app
```

- [ ] **Step 2: `src/app/MainWindow.cpp` ersetzen**

```cpp
#include "app/MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QStackedWidget>
#include <QThread>

#include <memory>

#include "app/ConnectedPage.h"
#include "app/SettingsStore.h"
#include "app/pairing/CertificateChangedPage.h"
#include "app/pairing/CertificatePage.h"
#include "app/pairing/PairingCodePage.h"
#include "app/pairing/ServerUrlPage.h"
#include "bridge/AuthBridge.h"
#include "core/api/CurlTransport.h"
#include "core/auth/SessionPaths.h"
#include "core/util/Logger.h"
#include "core/util/WindowsCredentialStore.h"

namespace baludesk::app {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("BaluDesk %1").arg(QCoreApplication::applicationVersion()));
    resize(900, 600);

    buildPages();
    startAuthThread();
    wireBridge();

    // Erst jetzt fragen: die Verbindungen müssen stehen, sonst geht die
    // Antwort ins Leere.
    QMetaObject::invokeMethod(bridge_, "restoreSession", Qt::QueuedConnection);
}

MainWindow::~MainWindow() {
    // Der Thread muss vor dem Fenster enden. deleteLater auf finished sorgt
    // dafür, dass die Bridge in ihrem eigenen Thread stirbt - dort, wo ihre
    // Mitglieder entstanden sind.
    authThread_->quit();
    authThread_->wait();
}

void MainWindow::buildPages() {
    stack_ = new QStackedWidget(this);

    serverUrlPage_ = new ServerUrlPage(this);
    certificatePage_ = new CertificatePage(this);
    pairingCodePage_ = new PairingCodePage(this);
    certificateChangedPage_ = new CertificateChangedPage(this);
    connectedPage_ = new ConnectedPage(this);

    stack_->addWidget(serverUrlPage_);
    stack_->addWidget(certificatePage_);
    stack_->addWidget(pairingCodePage_);
    stack_->addWidget(certificateChangedPage_);
    stack_->addWidget(connectedPage_);

    stack_->setCurrentWidget(serverUrlPage_);
    setCentralWidget(stack_);
}

void MainWindow::startAuthThread() {
    SettingsStore settings;

    bridge_ = new bridge::AuthBridge(std::make_shared<core::CurlTransport>(),
                                     std::make_unique<core::WindowsCredentialStore>(),
                                     core::defaultSessionPath(),
                                     settings.deviceId(),
                                     SettingsStore::deviceName());

    authThread_ = new QThread(this);
    bridge_->moveToThread(authThread_);
    connect(authThread_, &QThread::finished, bridge_, &QObject::deleteLater);
    authThread_->start();
}

void MainWindow::wireBridge() {
    // Von der Oberfläche zur Bridge. Weil beide in verschiedenen Threads
    // leben, stellt Qt diese Aufrufe selbsttätig in die Warteschlange - der
    // GUI-Thread wartet nie.
    connect(serverUrlPage_, &ServerUrlPage::connectRequested,
            bridge_, &bridge::AuthBridge::connectToServer);
    connect(certificatePage_, &CertificatePage::confirmed,
            bridge_, &bridge::AuthBridge::confirmCertificate);
    connect(certificatePage_, &CertificatePage::rejected,
            bridge_, &bridge::AuthBridge::rejectCertificate);
    connect(pairingCodePage_, &PairingCodePage::cancelled,
            bridge_, &bridge::AuthBridge::cancelPairing);
    connect(connectedPage_, &ConnectedPage::logoutRequested,
            bridge_, &bridge::AuthBridge::logout);
    connect(certificateChangedPage_, &CertificateChangedPage::repairRequested,
            bridge_, &bridge::AuthBridge::logout);
    connect(certificateChangedPage_, &CertificateChangedPage::quitRequested,
            qApp, &QApplication::quit);

    // Beim Klick auf „Verbinden" den Knopf sperren - der Aufruf kann bis zu
    // 30 Sekunden dauern, und ein zweiter Klick startete einen zweiten
    // Vorgang.
    connect(serverUrlPage_, &ServerUrlPage::connectRequested,
            this, [this]() { serverUrlPage_->setBusy(true); });

    // Von der Bridge zur Oberfläche.
    connect(bridge_, &bridge::AuthBridge::noSession, this, [this]() {
        serverUrlPage_->setBusy(false);
        stack_->setCurrentWidget(serverUrlPage_);
    });

    connect(bridge_, &bridge::AuthBridge::sessionEstablished, this,
            [this](const QString& serverUrl, const QString& username) {
                pairingCodePage_->stop();
                serverUrlPage_->setBusy(false);
                connectedPage_->clearOffline();
                connectedPage_->showConnection(serverUrl, username);
                stack_->setCurrentWidget(connectedPage_);
            });

    connect(bridge_, &bridge::AuthBridge::serverUnreachable, this,
            [this](const QString& message) {
                if (stack_->currentWidget() == connectedPage_) {
                    connectedPage_->showOffline(message);
                    return;
                }
                serverUrlPage_->showProblem(message);
                stack_->setCurrentWidget(serverUrlPage_);
            });

    connect(bridge_, &bridge::AuthBridge::serverUrlRejected,
            serverUrlPage_, &ServerUrlPage::showProblem);

    connect(bridge_, &bridge::AuthBridge::certificateConfirmationRequired, this,
            [this](const QString& fingerprint, const QString& reason) {
                certificatePage_->showCertificate(fingerprint, reason);
                stack_->setCurrentWidget(certificatePage_);
            });

    connect(bridge_, &bridge::AuthBridge::certificateChanged, this,
            [this](const QString& serverUrl, const QString& reason) {
                pairingCodePage_->stop();
                certificateChangedPage_->showChange(serverUrl, reason);
                stack_->setCurrentWidget(certificateChangedPage_);
            });

    connect(bridge_, &bridge::AuthBridge::pairingCodeReady, this,
            [this](const QString& code, const QString& url, int expiresInSeconds) {
                pairingCodePage_->showCode(code, url, expiresInSeconds);
                stack_->setCurrentWidget(pairingCodePage_);
            });

    connect(bridge_, &bridge::AuthBridge::pairingNotice,
            pairingCodePage_, &PairingCodePage::showNotice);

    connect(bridge_, &bridge::AuthBridge::pairingFailed, this,
            [this](const QString& message) {
                pairingCodePage_->stop();
                serverUrlPage_->showProblem(message);
                stack_->setCurrentWidget(serverUrlPage_);
            });

    connect(bridge_, &bridge::AuthBridge::pairingCancelled, this, [this]() {
        pairingCodePage_->stop();
        serverUrlPage_->setBusy(false);
        serverUrlPage_->clearMessages();
        stack_->setCurrentWidget(serverUrlPage_);
    });

    connect(bridge_, &bridge::AuthBridge::reauthRequired, this,
            [this](const QString& message) {
                serverUrlPage_->showNotice(message);
                stack_->setCurrentWidget(serverUrlPage_);
            });

    connect(bridge_, &bridge::AuthBridge::loggedOut, this, [this]() {
        serverUrlPage_->setBusy(false);
        serverUrlPage_->clearMessages();
        stack_->setCurrentWidget(serverUrlPage_);
    });

    connect(bridge_, &bridge::AuthBridge::logoutIncomplete, this,
            [this](const QString& message) {
                // Auch unvollständig abgemeldet heisst abgemeldet - die
                // Ansicht wechselt, die Einschränkung wird benannt.
                serverUrlPage_->setBusy(false);
                serverUrlPage_->showProblem(message);
                stack_->setCurrentWidget(serverUrlPage_);
            });
}

}  // namespace baludesk::app
```

- [ ] **Step 3: `src/app/main.cpp` — den Platzhaltertext ersetzen**

Die Datei bleibt bis auf eine Zeile unverändert. Die Protokollzeile

```cpp
    baludesk::core::Logger::info("Abfrageintervall: {} s", config.pollIntervalSeconds);
```

bleibt stehen; darunter ergänzen:

```cpp
    baludesk::core::Logger::info("Sitzungsablage: {}", baludesk::core::defaultSessionPath());
```

und oben den Include ergänzen:

```cpp
#include "core/auth/SessionPaths.h"
```

Damit steht der tatsächlich benutzte Ablageort im Protokoll — die Übergabe aus 1b nennt das Auseinanderlaufen dieses Pfads als stillen Fehlermodus, und ein Protokolleintrag macht ihn in einer Minute statt in einer Stunde auffindbar.

- [ ] **Step 4: Bauen und starten**

```powershell
cmake --preset msvc2022
cmake --build --preset debug
ctest --preset debug
```

Erwartet: 176 Tests grün, keine Warnungen aus eigenem Code.

```powershell
.\build\msvc2022\Debug\baludesk.exe
```

Erwartet: Das Fenster erscheint. Ist bereits eine Sitzung aus einem früheren Prüfprogramm-Lauf abgelegt, steht dort binnen einer Sekunde „Verbunden mit … als …"; sonst die Eingabe der Serveradresse.

- [ ] **Step 5: Committen**

```powershell
git add src/app/MainWindow.h src/app/MainWindow.cpp src/app/main.cpp
git commit -m @'
feat(app): Hauptfenster verdrahtet Stapel und Arbeiterthread

Die einzige Stelle im Programm, an der ein Thread entsteht. Die AuthBridge
weiss nichts davon; sie wird hier verschoben, und alles danach laeuft ueber
die Ereigniswarteschlange.

Der Platzhalter aus Stufe 0 ist damit weg.
'@
```

---

## Task 9: Abnahme gegen den laufenden Server und Dokumentation

Was nicht gegen den echten Server gefahren wurde, gilt nicht als belegt. Die Stufen 1a und 1b wurden so abgenommen; diese auch.

**Files:**
- Modify: `docs/superpowers/specs/2026-07-26-baludesk-qt-design.md` (Abschnitt 14 ergänzen)
- Create: `docs/superpowers/uebergabe-an-stufe-2.md`
- Delete: `docs/superpowers/uebergabe-an-stufe-1c.md`

---

- [ ] **Step 1: Die Abnahmeliste abfahren**

Jede Zeile von Hand, mit laufendem BaluHost. Ergebnisse mitschreiben — sie gehen gleich in die Spec.

| # | Prüfung | Erwartung |
|---|---|---|
| 1 | Abmelden, falls verbunden. Dann `http://nas.example.com` eingeben | abgelehnt **vor** jedem Netzkontakt, Meldung unter dem Feld |
| 2 | `ftp://nas.example.com` eingeben | abgelehnt, eigene Meldung |
| 3 | `nas.example.com` eingeben | Code erscheint ohne Zertifikatsnachfrage |
| 4 | Während der Codeanzeige das Fenster verschieben und in der Grösse ändern | Fenster reagiert flüssig — der Beleg, dass der Arbeiterthread trägt |
| 5 | „Im Browser öffnen", anmelden, Code eingeben, freigeben | Ansicht wechselt selbsttätig auf „Verbunden mit … als …" |
| 6 | Geräteliste der Web-Oberfläche | zeigt den Rechnernamen als aktives Desktop-Gerät |
| 7 | Anwendung beenden und neu starten | sofort verbunden, ohne Paaren — **das Abnahmekriterium der Stufe 1** |
| 8 | `session.json` ansehen | nur Adresse, Benutzername, Pin — **kein Token** |
| 9 | Abmelden, neu paaren, Geräteliste erneut ansehen | **derselbe** Eintrag, kein zweiter |
| 10 | Abmelden. `nas.local` eingeben | Fingerabdruck erscheint mit Grund |
| 11 | Fingerabdruck mit dem aus der Abnahme der Stufe 1b vergleichen | zeichengenau gleich |
| 12 | Bestätigen und freigeben | gepaart; `session.json` trägt jetzt einen Pin |
| 13 | Beenden, in `session.json` **ein Zeichen des Pins verfälschen**, starten | Seite „Das Zertifikat hat sich geändert" — kein stiller Abbruch |
| 14 | Dort „Abmelden und neu verbinden" | Sitzung entfernt, Eingabe der Serveradresse |
| 15 | Auf der Zertifikatsseite „Abbrechen" | zurück zur Eingabe, nichts angeheftet |
| 16 | Während der Codeanzeige „Abbrechen" | zurück zur Eingabe |
| 17 | Server anhalten, Anwendung starten (mit gültiger Sitzung) | bleibt verbunden, zeigt den Offline-Hinweis — **verlangt nicht Neupaaren** |
| 18 | Protokolldatei ansehen | „Sitzungsablage: …" nennt denselben Pfad, den auch `baludesk_probe status` benutzt |

Zeile 17 ist die wichtigste nach 7: Spec Abschnitt 10 macht den schlafenden Server zum Normalzustand. Ein Client, der daraus „bitte neu paaren" macht, ist im Alltag unbrauchbar.

Zeile 13 ist der Beleg für Entscheidung 2.3 der Stufen-Spec, und der einzige Weg, sie ohne Zertifikatswechsel am Server zu prüfen.

- [ ] **Step 2: Falls eine Zeile fehlschlägt**

Nicht weitermachen, sondern die Ursache beheben und die Liste **von vorn** abfahren. Eine Abnahme mit einer offenen Zeile ist keine.

- [ ] **Step 3: Abschnitt 14 der übergeordneten Spec ergänzen**

In `docs/superpowers/specs/2026-07-26-baludesk-qt-design.md` hinter dem Block „Stufe 1b" anfügen — die tatsächlichen Ergebnisse aus Step 1, nicht die Erwartungen:

```markdown
### Stufe 1c — Pairing-Oberfläche und Token-Erneuerung (abgenommen 2026-07-__)

176 Tests. Die Abnahme wurde gegen den laufenden Server gefahren, weil der
entscheidende Schritt einen Menschen am Browser verlangt.

| Prüfung | Ergebnis |
|---|---|
| … | … |

Damit ist das Abnahmekriterium aus Abschnitt 12 erstmals über die Anwendung
selbst erreicht statt über das Prüfprogramm.
```

Ausserdem in Abschnitt 5.1 die Verzeichnisstruktur um `bridge/` und `app/pairing/` nachziehen, soweit sie von der tatsächlichen abweicht.

- [ ] **Step 4: `docs/superpowers/uebergabe-an-stufe-2.md` schreiben**

Nach dem Muster von `uebergabe-an-stufe-1c.md`, mit vier Abschnitten:

- **Was fertig dasteht und benutzt werden will** — Tabelle. Mindestens: `AuthenticatedClient` (jeder Sync-Aufruf gehört hindurch, nicht an `HttpClient` vorbei), `AuthBridge` als Muster für `SyncEngineBridge`, `TrustNegotiation`, `SettingsStore::deviceId`.
- **Fallen, die schon jemanden gekostet hätten** — mindestens: `resetPairing()` löscht `pendingServerUrl_`, deshalb müssen Adresse und Benutzername vor dem Aufruf gesichert werden; `baludesk_bridge` darf `Qt6::Widgets` nicht linken, sonst rutscht ein Dialog in den Arbeiterthread; die Deklarationsreihenfolge von `secrets_` und `repository_` in `AuthBridge`.
- **Was Stufe 2 neu entscheiden muss** — mindestens: ob der Sync-Thread derselbe wie der Auth-Thread ist oder ein eigener (Spec 7.1 nennt Koordinator- und Transfer-Threads getrennt); wie `authRequired` aus Spec 6.3 mit `reauthRequired` der `AuthBridge` zusammengeht; wer den `AuthenticatedClient` besitzt, wenn zwei Bridges ihn brauchen.
- **Bekannte offene Punkte** — die aus `uebergabe-an-stufe-1c.md` übernehmen, die nicht erledigt wurden: `PairingClient` nimmt ein fehlendes `user`-Objekt still hin; kein Test für einen unerwarteten HTTP-Status beim Abruf; `intField` fällt bei `"interval": 5.0` still auf die Vorgabe zurück; `windeployqt` ohne MSVC-Laufzeitbibliotheken (Stufe 7); kein `compile_commands.json`; Version an drei Stellen; das Include-Leck von vcpkg nach `src/app/`. Dazu neu, was beim Bauen auffiel.

**Erledigt und deshalb nicht zu übernehmen:** die Extraktion der Vertrauensentscheidung, der fehlende Weg zurück bei veraltetem Pin, das Zeitfenster beim Speichern, `defaultSessionPath()` mit `"."`, die nicht ausgewerteten `std::error_code`, `clear()` mit `void`, die `SecretStore&`-Lebensdauer.

- [ ] **Step 5: Die alte Übergabe entfernen**

```powershell
git rm docs/superpowers/uebergabe-an-stufe-1c.md
```

Sie ist vollständig aufgegangen — ihre Punkte sind entweder erledigt oder in der neuen Übergabe weitergeführt. Zwei Übergaben nebeneinander stehen zu lassen hiesse, dass beim nächsten Mal jemand die falsche liest.

- [ ] **Step 6: Committen**

```powershell
git add docs/superpowers
git commit -m @'
docs: Stufe 1c abgenommen, Uebergabe an Stufe 2

Abnahme gegen den laufenden Server gefahren, 176 Tests gruen. Das
Abnahmekriterium der Stufe 1 ist erstmals ueber die Anwendung selbst
erreicht statt ueber das Pruefprogramm.
'@
```

---

## Selbstprüfung des Plans

Durchgeführt nach dem Schreiben, gegen die Stufen-Spec:

**Spec-Abdeckung.** Jeder Abschnitt der Spec hat einen Task: 2.1 Ansichtswechsel → Tasks 7 und 8 · 2.2 Arbeiterthread → Tasks 5 und 8 · 2.3 veralteter Pin → Tasks 5, 7 (CertificateChangedPage) und 8 · 2.4 Erneuerung in core → Task 3 · 2.5 Gerätekennung → Task 4 · 2.6 `saveAccessToken` → Task 2 · 3.1 `TrustNegotiation` → Task 1 · 3.2 `AuthenticatedClient` → Task 3 · 4.1 Start → Task 5 · 4.2 Paaren → Task 6 · 4.3 Abmelden → Task 5 · 4.4 Signale und Slots → Tasks 5 und 6 · 5 Fehlerbehandlung → über die Tasks 5 bis 8 verteilt, jede Zeile der Tabelle hat einen Zweig · 6 Tests → Tasks 1, 2, 3, 4, 5, 6, 7 · 7 offene Punkte → Task 2 (drei davon) und Task 1 (`tools/probe`) · 9 Abnahme → Task 9.

**Zwei bewusste Abweichungen**, beide oben im Kopf begründet: `certificateChanged` trägt einen Grund mit, und `pollNow()` ist öffentlich.

**Typkonsistenz.** `AuthOutcome` wird in Task 3 definiert und in Task 5 in `restoreSession()` verwendet — alle fünf Werte sind dort abgedeckt. `TrustState` wird in Task 1 definiert und in Task 6 verwendet — ebenfalls alle sechs. `SessionRepository::clear()` liefert ab Task 2 `bool`; Task 5 wertet ihn aus, `tools/probe` wurde in Task 2 mitgezogen. `saveAccessToken` entsteht in Task 2 und wird in Task 3 gebraucht — die Reihenfolge stimmt. `FakeSecretStore::failOnEraseNumber` entsteht in Task 2 und wird in Task 5 benutzt. Die Seitenmethoden aus Task 7 (`showCertificate`, `showChange`, `showConnection`, `showCode`, `stop`) heissen in Task 8 genauso.

**Reihenfolgeabhängigkeiten.** 1 → 6 (`TrustNegotiation`), 2 → 3 (`saveAccessToken`), 2 → 5 (`clear`, `failOnEraseNumber`), 3 → 5 (`AuthenticatedClient`), 4 → 8 (`deviceId`), 5 → 6 (leere Slots), 7 → 8 (Seiten). Keine Rückwärtskante.

