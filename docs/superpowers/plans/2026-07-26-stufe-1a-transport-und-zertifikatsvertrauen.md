# BaluDesk QT — Stufe 1a (Transport und Zertifikatsvertrauen) Implementierungsplan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** BaluDesk kann eine HTTPS-Anfrage an BaluHost stellen — mit strenger Zertifikatsprüfung im Normalfall und einmalig bestätigtem Zertifikat-Pin im Heimnetz — und Zugangsdaten sicher im Windows Credential Manager ablegen.

**Architecture:** Alles Neue liegt in `baludesk_core` und bleibt Qt-frei. `HttpClient` wird an einer `Transport`-Naht getrennt: die Anfragenzusammenstellung und Antwortauswertung sind reine Logik und vollständig testbar, der curl-Aufruf steckt hinter einer schmalen Schnittstelle. `TlsTrust` rechnet Pins und Fingerabdrücke mit OpenSSL. `CredentialStore` kommt aus dem Alt-Client, mit korrigierter UTF-8-Wandlung.

**Tech Stack:** C++17 · libcurl 8.21 **mit OpenSSL-Backend** · OpenSSL 3.x · nlohmann/json · spdlog · GoogleTest · Qt 6.8.0 (nur in `app/`)

**Spec:** `docs/superpowers/specs/2026-07-26-baludesk-qt-design.md` — Abschnitt 2.9 (TLS), 2.6 (Pairing), 9 (Server-Schnittstelle), 12 Stufe 1

**Vorgänger:** `docs/superpowers/plans/2026-07-26-stufe-0-geruest.md`, abgeschlossen, 25 Tests grün

**Nachfolger:** Stufe 1b bringt den Gerätecode-Ablauf, die Sitzungsspeicherung, die Dialoge und das Abmelden. Dieser Plan endet, sobald eine gesicherte HTTPS-Anfrage nachweislich funktioniert.

## Global Constraints

Diese Vorgaben gelten für jede Aufgabe in diesem Plan.

- **C++-Standard:** C++17, `CMAKE_CXX_EXTENSIONS OFF`
- **`cmake_minimum_required(VERSION 3.25)`** — nicht niedriger
- **Qt:** exakt `F:/Qt/6.8.0/msvc2022_64`, Module Core, Gui, Widgets. Kein QML.
- **vcpkg:** Installation unter `F:/vcpkg`, Triplet `x64-windows`, Manifest-Modus
- **`baludesk_core` darf niemals Qt linken.** Die Absicherung steht in der Wurzel-`CMakeLists.txt` nach `add_subdirectory(tests)` und prüft `LINK_LIBRARIES` und `INTERFACE_LINK_LIBRARIES`. Alles in diesem Plan außer Task 1 landet in `core/` und muss Qt-frei bleiben.
- **Namensräume:** `baludesk::core` für alles unter `src/core/`, `baludesk::app` für `src/app/`
- **Include-Pfade immer vollständig ab `src/`**, also `#include "core/api/HttpClient.h"`, nie relativ mit `../`
- **Sprache:** Bezeichner und Dateinamen englisch, Kommentare und Protokolltexte deutsch
- **Umlaute:** In allen Dateien echte UTF-8-Zeichen (`ü`, `ö`, `ä`, `ß`), gespeichert als UTF-8 **ohne BOM**. Keine Behelfsschreibung. **Einzige Ausnahme: Git-Commit-Nachrichten bleiben ASCII**, weil PowerShell-5.1-Here-Strings Umlaute verstümmeln.
- **Generator:** `Visual Studio 17 2022`, x64. Warnungen `/W4 /permissive- /utf-8`.
- **Shell ist PowerShell 5.1:** kein `&&`, mit `;` verketten. Pfade immer quoten.
- **Nicht in Stufe 1a:** Gerätecode-Ablauf, Sitzungsspeicherung, Dialoge, Abmelden, Sync, Datenbank, Infobereichssymbol, Dashboard

## Server-Vertrag, soweit hier gebraucht

Gegen BaluHost v1.38.0 verifiziert am 2026-07-26.

| Zweck | Endpunkt |
|---|---|
| Erreichbarkeitsprüfung, ohne Anmeldung | `GET /api/health` |
| Angemeldeten Nutzer lesen | `GET /api/auth/me` (Bearer-Token) |

Der vollständige Pairing-Vertrag steht im Plan zu Stufe 1b.

---

## Dateistruktur nach Abschluss

```
src/core/
├── CMakeLists.txt            # + OpenSSL, + neue Quelldateien
├── api/
│   ├── HttpTypes.h           # HttpMethod, HttpRequest, HttpResponse, HttpError
│   ├── Transport.h           # reine Schnittstelle — die Testnaht
│   ├── CurlTransport.h/.cpp  # libcurl dahinter, nicht unit-getestet
│   ├── HttpClient.h/.cpp     # Basis-URL, Kopfzeilen, JSON, Fehlerdeutung
│   └── TlsPolicy.h           # Vertrauensmodus als Werttyp
└── util/
    ├── TlsTrust.h/.cpp       # SPKI-Pin und Fingerabdruck aus einem PEM
    └── CredentialStore.h/.cpp # Windows Credential Manager

tests/core/
├── tls_trust_test.cpp
├── credential_store_test.cpp
├── http_client_test.cpp      # gegen FakeTransport
└── data/                     # zwei Testzertifikate, in Task 2 erzeugt

src/app/
└── main.cpp                  # + qInstallMessageHandler
```

**Warum die `Transport`-Naht:** Ein `HttpClient`, der curl direkt aufruft, ist nur mit einem echten Server testbar. Getrennt ist die gesamte Logik — Pfadzusammensetzung, Kopfzeilen, JSON-Auswertung, Fehlerklassifikation — gegen einen Fake prüfbar, und nur der schmale `CurlTransport` bleibt der manuellen Abnahme vorbehalten. Dasselbe Muster trägt später den `Reconciler` der Stufe 2.

---

## Task 1: vcpkg auf OpenSSL umstellen und Qt-Meldungen ins Protokoll leiten

**Files:**
- Modify: `vcpkg.json`
- Modify: `src/app/main.cpp`
- Modify: `src/app/CMakeLists.txt`
- Create: `src/app/QtLogBridge.h`
- Create: `src/app/QtLogBridge.cpp`
- Test: `tests/app/qt_log_bridge_test.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `baludesk::core::Logger` aus Stufe 0 — Statiken `init(const std::string&, std::size_t, std::size_t)`, `shutdown()`, `isInitialized()`, `logFilePath()`, `flush()` und die Vorlagen `debug`/`info`/`warn`/`error`
- Produces: `void baludesk::app::installQtLogBridge()`; curl mit OpenSSL-Backend für alle folgenden Tasks

---

- [ ] **Step 1: vcpkg-Baseline bestätigen**

Ohne Baseline treiben die Abhängigkeitsversionen mit dem Stand von `F:\vcpkg`. Das wurde im Abschlussreview der Stufe 0 angemerkt und wird jetzt nachgeholt, weil dieselbe Datei ohnehin angefasst wird.

Die Baseline steht in Step 2 bereits eingetragen — es ist der Stand von `F:\vcpkg` am 2026-07-26. Prüfe, dass er noch stimmt:

```powershell
git -C F:\vcpkg rev-parse HEAD
```

Erwartet: `40f3c709db80acf154ac4b17a1f83c564ebd022e`.

Weicht die Ausgabe ab, wurde vcpkg zwischenzeitlich aktualisiert. Dann **die ausgegebene SHA in Step 2 eintragen** statt der vorgegebenen — eine Baseline, die auf einen nicht vorhandenen Stand zeigt, lässt das Konfigurieren scheitern.

- [ ] **Step 2: `vcpkg.json` auf OpenSSL umstellen**

`ssl` zieht unter Windows das Feature `sspi` und damit **Schannel**. Schannel kann zwar `CURLOPT_PINNEDPUBLICKEY`, liefert aber nur das Zertifikat als PEM — den öffentlichen Schlüssel daraus zu gewinnen hieße ASN.1 von Hand zu parsen. Mit OpenSSL rechnet die Bibliothek den Hash selbst, und `CURLSSLOPT_NATIVE_CA` lässt trotzdem den Windows-Wurzelspeicher gelten.

`default-features` muss abgeschaltet werden, sonst baut vcpkg beide Backends nebeneinander. Damit fällt auch `non-http` weg — die Protokolle jenseits von HTTP und HTTPS braucht dieser Client nicht, und weniger Angriffsfläche ist ein Gewinn.

```json
{
  "$schema": "https://raw.githubusercontent.com/microsoft/vcpkg-tool/main/docs/vcpkg.schema.json",
  "name": "baludesk-qt",
  "version-string": "0.1.0",
  "description": "Desktop-Synchronisationsclient für BaluHost NAS",
  "builtin-baseline": "40f3c709db80acf154ac4b17a1f83c564ebd022e",
  "dependencies": [
    {
      "name": "curl",
      "default-features": false,
      "features": [ "openssl" ]
    },
    "openssl",
    "sqlite3",
    "nlohmann-json",
    "spdlog",
    "gtest"
  ]
}
```

- [ ] **Step 3: Neu konfigurieren und das Backend belegen**

```powershell
cmake --preset msvc2022
```

Erwartet: vcpkg baut OpenSSL. Das dauert beim ersten Mal mehrere Minuten — **im Vordergrund abwarten, nicht in eine Hintergrund-Shell legen.**

Danach prüfen, dass das Backend wirklich gewechselt hat:

```powershell
Get-ChildItem "build\msvc2022\vcpkg_installed\x64-windows\bin" -Filter "*.dll" | Select-Object -ExpandProperty Name
```

Erwartet: `libcrypto-3-x64.dll` und `libssl-3-x64.dll` sind jetzt vorhanden. Fehlen sie, ist curl weiter gegen Schannel gebaut und Task 2 kann nicht funktionieren — dann melden statt weitermachen.

- [ ] **Step 4: Den fehlschlagenden Test schreiben**

Datei `tests/app/qt_log_bridge_test.cpp`:

```cpp
#include "app/QtLogBridge.h"

#include <gtest/gtest.h>

#include <QDebug>
#include <QString>
#include <QTemporaryDir>

#include <filesystem>
#include <fstream>
#include <string>

#include "core/util/Logger.h"

namespace fs = std::filesystem;
using baludesk::core::Logger;

namespace {

std::string readWholeFile(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

}  // namespace

class QtLogBridgeTest : public ::testing::Test {
protected:
    void TearDown() override { Logger::shutdown(); }
};

TEST_F(QtLogBridgeTest, ForwardsQtWarningsIntoTheLogFile) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    Logger::init(directory.path().toStdString(), 1024 * 1024, 3);
    baludesk::app::installQtLogBridge();

    qWarning() << "Testmeldung aus Qt";
    Logger::flush();

    const std::string contents = readWholeFile(Logger::logFilePath());
    EXPECT_NE(contents.find("Testmeldung aus Qt"), std::string::npos);
    EXPECT_NE(contents.find("[warning]"), std::string::npos);
}

TEST_F(QtLogBridgeTest, ForwardsQtCriticalAsError) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    Logger::init(directory.path().toStdString(), 1024 * 1024, 3);
    baludesk::app::installQtLogBridge();

    qCritical() << "Schwerer Qt-Fehler";
    Logger::flush();

    const std::string contents = readWholeFile(Logger::logFilePath());
    EXPECT_NE(contents.find("Schwerer Qt-Fehler"), std::string::npos);
    EXPECT_NE(contents.find("[error]"), std::string::npos);
}
```

- [ ] **Step 5: `tests/CMakeLists.txt` erweitern**

In der Quelldateiliste von `baludesk_app_tests` ergänzen — die Datei sonst unverändert lassen, insbesondere die Reihenfolge von `find_package(Qt6 ...)` und `qt_standard_project_setup()` **vor** den `add_executable()`:

```cmake
add_executable(baludesk_app_tests
    app/settings_store_test.cpp
    app/qt_log_bridge_test.cpp
)
```

- [ ] **Step 6: Test laufen lassen und Fehlschlag bestätigen**

```powershell
cmake --build --preset debug
```

Erwartet: FEHLER — `Cannot open include file: 'app/QtLogBridge.h'`.

- [ ] **Step 7: `src/app/QtLogBridge.h` schreiben**

```cpp
#pragma once

namespace baludesk::app {

// Leitet Qts eigene Meldungen (qDebug, qWarning, qCritical, qFatal) in den
// Logger um.
//
// Ohne das verschwinden sie spurlos: das Programm wird als WIN32-Anwendung
// gebaut, hat also keine Konsole, und Qt schreibt seine Diagnose sonst nach
// stderr. Genau dort landen aber die Meldungen, die bei Netzwerk- und
// Einstellungsproblemen weiterhelfen.
//
// Darf erst nach Logger::init() aufgerufen werden. Ein Aufruf davor ist
// unschädlich, die Meldungen verpuffen dann still.
void installQtLogBridge();

}  // namespace baludesk::app
```

- [ ] **Step 8: `src/app/QtLogBridge.cpp` schreiben**

`qFatal` bricht nach der Behandlung ohnehin ab; die Meldung wird vorher noch herausgeschrieben und das Protokoll geleert, damit sie die Datei erreicht.

```cpp
#include "app/QtLogBridge.h"

#include <QByteArray>
#include <QMessageLogContext>
#include <QString>
#include <QtGlobal>

#include "core/util/Logger.h"

namespace baludesk::app {
namespace {

void forwardToLogger(QtMsgType type, const QMessageLogContext& context, const QString& message) {
    const QByteArray utf8 = message.toUtf8();
    const std::string text(utf8.constData(), static_cast<std::size_t>(utf8.size()));

    // Herkunft anhängen, wenn Qt sie mitliefert. In Release-Builds fehlt sie
    // meist, deshalb nicht darauf verlassen.
    std::string origin;
    if (context.file != nullptr && context.line > 0) {
        origin = std::string(" (") + context.file + ":" + std::to_string(context.line) + ")";
    }

    switch (type) {
        case QtDebugMsg:
            core::Logger::debug("Qt: {}{}", text, origin);
            break;
        case QtInfoMsg:
            core::Logger::info("Qt: {}{}", text, origin);
            break;
        case QtWarningMsg:
            core::Logger::warn("Qt: {}{}", text, origin);
            break;
        case QtCriticalMsg:
            core::Logger::error("Qt: {}{}", text, origin);
            break;
        case QtFatalMsg:
            core::Logger::error("Qt (fatal): {}{}", text, origin);
            core::Logger::flush();
            break;
    }
}

}  // namespace

void installQtLogBridge() {
    qInstallMessageHandler(forwardToLogger);
}

}  // namespace baludesk::app
```

- [ ] **Step 9: `src/app/CMakeLists.txt` erweitern**

Nur die Quelldateiliste von `baludesk_app_lib` ergänzen, alles Übrige unverändert lassen:

```cmake
add_library(baludesk_app_lib STATIC
    MainWindow.cpp
    SettingsStore.cpp
    QtLogBridge.cpp
)
```

- [ ] **Step 10: Tests laufen lassen und grün bestätigen**

```powershell
cmake --build --preset debug
ctest --preset debug
```

Erwartet: 27 Tests, alle bestanden.

- [ ] **Step 11: In `src/app/main.cpp` verdrahten**

Der Aufruf gehört **direkt nach** `Logger::init(...)` und **vor** die erste Zeile, die etwas protokolliert. Ergänze den Include

```cpp
#include "app/QtLogBridge.h"
```

und unmittelbar nach dem `Logger::init(...)`-Aufruf:

```cpp
    // Ab hier landen auch Qts eigene Meldungen im Protokoll statt im Nichts.
    baludesk::app::installQtLogBridge();
```

- [ ] **Step 12: Bauen, starten, Protokoll prüfen**

```powershell
cmake --build --preset debug
& "build\msvc2022\src\app\Debug\baludesk.exe"
```

Fenster schließen, dann:

```powershell
Get-Content "$env:LOCALAPPDATA\BaluHost\BaluDesk\logs\baludesk.log" -Encoding UTF8
```

Erwartet: die bekannten drei Zeilen. `-Encoding UTF8` ist nötig, weil PowerShell 5.1 die Datei sonst als ANSI liest und Umlaute als `Ã¼` anzeigt — das wäre ein Anzeigefehler, kein echter.

- [ ] **Step 13: Committen**

```powershell
git add vcpkg.json src/app tests/CMakeLists.txt tests/app
git commit -m "build: switch curl to OpenSSL and route Qt messages into the log"
```

---

## Task 2: TlsTrust — Fingerabdruck und Pin aus einem Zertifikat

**Files:**
- Create: `src/core/util/TlsTrust.h`
- Create: `src/core/util/TlsTrust.cpp`
- Modify: `src/core/CMakeLists.txt`
- Create: `tests/core/data/test-cert-a.pem`
- Create: `tests/core/data/test-cert-b.pem`
- Test: `tests/core/tls_trust_test.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: nichts aus früheren Tasks dieses Plans
- Produces:
  - `struct baludesk::core::CertificateIdentity { std::string fingerprintSha256; std::string publicKeyPin; }`
  - `std::optional<CertificateIdentity> baludesk::core::identifyCertificate(const std::string& pem)`
  - `fingerprintSha256` ist die Großschreib-Hexdarstellung des SHA-256 über das DER-kodierte Zertifikat, in Doppelpunkt-getrennten Bytepaaren: `F4:B6:19:...` (95 Zeichen)
  - `publicKeyPin` ist das Format, das `CURLOPT_PINNEDPUBLICKEY` erwartet: `sha256//` gefolgt von Base64 des SHA-256 über den DER-kodierten SubjectPublicKeyInfo

---

- [ ] **Step 1: Zwei Testzertifikate erzeugen**

Zwei Fallen auf dieser Maschine, beide verifiziert:

1. **`MSYS_NO_PATHCONV=1` ist zwingend.** Ohne das wandelt die Git-Bash `/CN=...` in einen Windows-Pfad um und `openssl req` scheitert.
2. **Die Ausgabepfade müssen Windows-Form haben.** Das `openssl.exe` aus Git für Windows ist ein natives Programm und kann mit MSYS-Pfaden wie `/tmp/...` nichts anfangen.

In der Git-Bash, aus dem Repo-Wurzelverzeichnis:

```bash
mkdir -p tests/core/data
OUT="$(pwd -W)/tests/core/data"
MSYS_NO_PATHCONV=1 openssl req -x509 -newkey rsa:2048 \
  -keyout "$OUT/test-key-a.pem" -out "$OUT/test-cert-a.pem" \
  -days 36500 -nodes -subj "/CN=baludesk-test-a"
MSYS_NO_PATHCONV=1 openssl req -x509 -newkey rsa:2048 \
  -keyout "$OUT/test-key-b.pem" -out "$OUT/test-cert-b.pem" \
  -days 36500 -nodes -subj "/CN=baludesk-test-b"
rm -f "$OUT/test-key-a.pem" "$OUT/test-key-b.pem"
ls -la tests/core/data
```

Die privaten Schlüssel werden sofort gelöscht — für die Tests wird nur der öffentliche Teil gebraucht, und ein privater Schlüssel im Repo ist unnötiger Ballast. Laufzeit 36500 Tage, damit die Tests nicht eines Tages an einem abgelaufenen Zertifikat scheitern.

Erwartet: `test-cert-a.pem` und `test-cert-b.pem` liegen in `tests/core/data`, jeweils rund 1,2 KB, beginnend mit `-----BEGIN CERTIFICATE-----`.

- [ ] **Step 2: Den fehlschlagenden Test schreiben**

Es stehen bewusst **keine fest eingetragenen Erwartungswerte** im Test: Pin und Fingerabdruck hängen an den in Step 1 frisch erzeugten Zertifikaten und lassen sich beim Schreiben des Plans nicht vorwegnehmen. Geprüft werden stattdessen Form, Bestimmtheit und Unterscheidungskraft — das fängt jeden realistischen Fehler in der Berechnung ab. Die Verankerung gegen einen extern gerechneten Wert passiert in Step 7.

Datei `tests/core/tls_trust_test.cpp`:

```cpp
#include "core/util/TlsTrust.h"

#include <gtest/gtest.h>

#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using baludesk::core::identifyCertificate;

namespace {

// Der Pfad wird beim Übersetzen über eine Definition hereingereicht, damit die
// Tests unabhängig vom Arbeitsverzeichnis laufen.
std::string readTestData(const std::string& name) {
    const fs::path path = fs::path(BALUDESK_TEST_DATA_DIR) / name;
    std::ifstream stream(path, std::ios::binary);
    EXPECT_TRUE(stream.good()) << "Testdatei nicht lesbar: " << path.string();
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

bool isUpperHex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
}

}  // namespace

TEST(TlsTrustTest, IdentifiesAValidCertificate) {
    const auto identity = identifyCertificate(readTestData("test-cert-a.pem"));
    ASSERT_TRUE(identity.has_value());
    EXPECT_FALSE(identity->fingerprintSha256.empty());
    EXPECT_FALSE(identity->publicKeyPin.empty());
}

TEST(TlsTrustTest, FingerprintHasTheExpectedShape) {
    const auto identity = identifyCertificate(readTestData("test-cert-a.pem"));
    ASSERT_TRUE(identity.has_value());

    // 32 Bytes als Hexpaare mit Doppelpunkt dazwischen: 32*2 + 31 = 95 Zeichen.
    const std::string& fingerprint = identity->fingerprintSha256;
    ASSERT_EQ(fingerprint.size(), std::size_t{95});

    for (std::size_t i = 0; i < fingerprint.size(); ++i) {
        if ((i % 3) == 2) {
            EXPECT_EQ(fingerprint[i], ':') << "an Position " << i;
        } else {
            EXPECT_TRUE(isUpperHex(fingerprint[i]))
                << "an Position " << i << ": '" << fingerprint[i] << "'";
        }
    }
}

TEST(TlsTrustTest, PinUsesTheFormatCurlExpects) {
    const auto identity = identifyCertificate(readTestData("test-cert-a.pem"));
    ASSERT_TRUE(identity.has_value());

    const std::string& pin = identity->publicKeyPin;
    ASSERT_GT(pin.size(), std::size_t{8});
    EXPECT_EQ(pin.substr(0, 8), "sha256//");

    // SHA-256 sind 32 Bytes; Base64 davon ist 44 Zeichen mit einem '=' am Ende.
    const std::string encoded = pin.substr(8);
    EXPECT_EQ(encoded.size(), std::size_t{44});
    EXPECT_EQ(encoded.back(), '=');
}

TEST(TlsTrustTest, IsDeterministic) {
    const std::string pem = readTestData("test-cert-a.pem");
    const auto first = identifyCertificate(pem);
    const auto second = identifyCertificate(pem);

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(first->fingerprintSha256, second->fingerprintSha256);
    EXPECT_EQ(first->publicKeyPin, second->publicKeyPin);
}

TEST(TlsTrustTest, DistinguishesDifferentCertificates) {
    const auto a = identifyCertificate(readTestData("test-cert-a.pem"));
    const auto b = identifyCertificate(readTestData("test-cert-b.pem"));

    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_NE(a->fingerprintSha256, b->fingerprintSha256);
    EXPECT_NE(a->publicKeyPin, b->publicKeyPin);
}

TEST(TlsTrustTest, RejectsGarbageInsteadOfCrashing) {
    EXPECT_FALSE(identifyCertificate("").has_value());
    EXPECT_FALSE(identifyCertificate("kein Zertifikat, nur Text").has_value());
    EXPECT_FALSE(identifyCertificate("-----BEGIN CERTIFICATE-----\nnicht base64\n-----END CERTIFICATE-----\n")
                     .has_value());
}

TEST(TlsTrustTest, RejectsATruncatedCertificate) {
    std::string pem = readTestData("test-cert-a.pem");
    ASSERT_GT(pem.size(), std::size_t{200});
    pem.resize(pem.size() / 2);

    EXPECT_FALSE(identifyCertificate(pem).has_value());
}
```

- [ ] **Step 3: `src/core/CMakeLists.txt` erweitern**

OpenSSL kommt hinzu. **Der Absicherungsblock steht nicht mehr in dieser Datei** — er liegt seit dem Abschlussreview der Stufe 0 in der Wurzel-`CMakeLists.txt`. Der Kopfkommentar dieser Datei bleibt unverändert stehen.

`find_package(OpenSSL REQUIRED)` ergänzen, `util/TlsTrust.cpp` in die Quelldateiliste, und OpenSSL verknüpfen:

```cmake
find_package(spdlog CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

add_library(baludesk_core STATIC
    util/AppConfig.cpp
    util/Logger.cpp
    util/TlsTrust.cpp
)

target_include_directories(baludesk_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(baludesk_core PUBLIC
    spdlog::spdlog
    OpenSSL::Crypto
)
```

Nur `OpenSSL::Crypto`, nicht `OpenSSL::SSL` — gerechnet wird mit Hashes und Zertifikatsstrukturen, den TLS-Handschlag führt curl.

- [ ] **Step 4: `tests/CMakeLists.txt` erweitern**

Die Testdatei aufnehmen und den Datenpfad hereinreichen. Alles Übrige in der Datei unverändert lassen:

```cmake
add_executable(baludesk_core_tests
    core/app_config_test.cpp
    core/logger_test.cpp
    core/tls_trust_test.cpp
)

target_compile_definitions(baludesk_core_tests PRIVATE
    BALUDESK_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/core/data"
)
```

- [ ] **Step 5: Test laufen lassen und Fehlschlag bestätigen**

```powershell
cmake --preset msvc2022
```

Erwartet: FEHLER beim Konfigurieren — `Cannot find source file: util/TlsTrust.cpp`.

- [ ] **Step 6: `src/core/util/TlsTrust.h` schreiben**

```cpp
#pragma once

#include <optional>
#include <string>

namespace baludesk::core {

// Die beiden Kennungen eines Serverzertifikats, die BaluDesk braucht.
struct CertificateIdentity {
    // SHA-256 über das DER-kodierte Zertifikat, als Hexpaare mit Doppelpunkt:
    // "F4:B6:19:0C:...". Das ist die Darstellung, die dem Nutzer beim
    // Bestätigen gezeigt wird - dieselbe, die auch Browser und die
    // openssl-Kommandozeile ausgeben, damit sie sich vergleichen lässt.
    std::string fingerprintSha256;

    // SHA-256 über den DER-kodierten SubjectPublicKeyInfo, base64-kodiert und
    // mit "sha256//" davor. Genau dieses Format erwartet libcurl bei
    // CURLOPT_PINNEDPUBLICKEY.
    //
    // Angeheftet wird der öffentliche Schlüssel und nicht das Zertifikat,
    // damit eine Erneuerung des Zertifikats mit gleichem Schlüssel den Pin
    // nicht bricht.
    std::string publicKeyPin;
};

// Liest ein Zertifikat im PEM-Format und berechnet beide Kennungen.
//
// Gibt nichts zurück, wenn der Text kein gültiges Zertifikat enthält - leer,
// abgeschnitten, kein PEM, kaputtes Base64. Wirft nie.
[[nodiscard]] std::optional<CertificateIdentity> identifyCertificate(const std::string& pem);

}  // namespace baludesk::core
```

- [ ] **Step 7: `src/core/util/TlsTrust.cpp` schreiben**

```cpp
#include "core/util/TlsTrust.h"

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <array>
#include <cstdio>
#include <memory>
#include <vector>

namespace baludesk::core {
namespace {

// Aufräumhilfen, damit kein OpenSSL-Objekt bei einem frühen return liegen bleibt.
struct BioDeleter {
    void operator()(BIO* bio) const noexcept { BIO_free(bio); }
};
struct X509Deleter {
    void operator()(X509* cert) const noexcept { X509_free(cert); }
};
struct OpenSslBufferDeleter {
    void operator()(unsigned char* buffer) const noexcept { OPENSSL_free(buffer); }
};

using BioPtr = std::unique_ptr<BIO, BioDeleter>;
using X509Ptr = std::unique_ptr<X509, X509Deleter>;
using OpenSslBufferPtr = std::unique_ptr<unsigned char, OpenSslBufferDeleter>;

std::string toColonHex(const unsigned char* data, unsigned int length) {
    static constexpr char kDigits[] = "0123456789ABCDEF";

    std::string result;
    result.reserve(length * 3);
    for (unsigned int i = 0; i < length; ++i) {
        if (i > 0) {
            result.push_back(':');
        }
        result.push_back(kDigits[(data[i] >> 4) & 0x0F]);
        result.push_back(kDigits[data[i] & 0x0F]);
    }
    return result;
}

std::string toBase64(const unsigned char* data, int length) {
    // EVP_EncodeBlock schreibt 4 Zeichen je 3 Eingabebytes plus Nullbyte.
    std::vector<unsigned char> encoded(static_cast<std::size_t>(((length + 2) / 3) * 4) + 1);
    const int written = EVP_EncodeBlock(encoded.data(), data, length);
    if (written <= 0) {
        return {};
    }
    return std::string(reinterpret_cast<const char*>(encoded.data()),
                       static_cast<std::size_t>(written));
}

}  // namespace

std::optional<CertificateIdentity> identifyCertificate(const std::string& pem) {
    if (pem.empty()) {
        return std::nullopt;
    }

    BioPtr bio(BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())));
    if (!bio) {
        return std::nullopt;
    }

    X509Ptr certificate(PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr));
    if (!certificate) {
        return std::nullopt;
    }

    CertificateIdentity identity;

    // Fingerabdruck: SHA-256 über das gesamte DER-kodierte Zertifikat.
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int digestLength = 0;
    if (X509_digest(certificate.get(), EVP_sha256(), digest.data(), &digestLength) != 1) {
        return std::nullopt;
    }
    identity.fingerprintSha256 = toColonHex(digest.data(), digestLength);

    // Pin: SHA-256 über den DER-kodierten SubjectPublicKeyInfo.
    X509_PUBKEY* publicKey = X509_get_X509_PUBKEY(certificate.get());
    if (publicKey == nullptr) {
        return std::nullopt;
    }

    unsigned char* rawSpki = nullptr;
    const int spkiLength = i2d_X509_PUBKEY(publicKey, &rawSpki);
    if (spkiLength <= 0 || rawSpki == nullptr) {
        return std::nullopt;
    }
    OpenSslBufferPtr spki(rawSpki);

    // EVP_Digest statt der freien Funktion SHA256(): letztere ist seit
    // OpenSSL 3.0 als veraltet markiert und löst unter /W4 eine Warnung aus.
    std::array<unsigned char, EVP_MAX_MD_SIZE> spkiDigest{};
    unsigned int spkiDigestLength = 0;
    if (EVP_Digest(spki.get(), static_cast<std::size_t>(spkiLength), spkiDigest.data(),
                   &spkiDigestLength, EVP_sha256(), nullptr) != 1) {
        return std::nullopt;
    }

    const std::string encoded = toBase64(spkiDigest.data(), static_cast<int>(spkiDigestLength));
    if (encoded.empty()) {
        return std::nullopt;
    }
    identity.publicKeyPin = "sha256//" + encoded;

    return identity;
}

}  // namespace baludesk::core
```

- [ ] **Step 8: Tests laufen lassen und grün bestätigen**

```powershell
cmake --preset msvc2022
cmake --build --preset debug
ctest --preset debug
```

Erwartet: 34 Tests, alle bestanden.

- [ ] **Step 9: Gegen ein unabhängiges Werkzeug verankern**

Die Tests prüfen Form und Bestimmtheit, aber nicht, ob die Zahlen dieselben sind, die der Rest der Welt berechnet. Das wird einmal von Hand belegt, gegen die openssl-Kommandozeile.

In der Git-Bash:

```bash
OUT="$(pwd -W)/tests/core/data"
echo -n "Pin laut openssl:          sha256//"
openssl x509 -in "$OUT/test-cert-a.pem" -pubkey -noout \
  | openssl pkey -pubin -outform der \
  | openssl dgst -sha256 -binary \
  | openssl enc -base64
openssl x509 -in "$OUT/test-cert-a.pem" -noout -fingerprint -sha256
```

Ein kleines Prüfprogramm gibt es nicht und soll es nicht geben — stattdessen die Werte aus dem Test heraus sichtbar machen. Ergänze **vorübergehend** in `IdentifiesAValidCertificate`:

```cpp
    std::cout << "Fingerabdruck: " << identity->fingerprintSha256 << "\n";
    std::cout << "Pin:           " << identity->publicKeyPin << "\n";
```

dazu `#include <iostream>`, dann

```powershell
cmake --build --preset debug
& "build\msvc2022\tests\Debug\baludesk_core_tests.exe" --gtest_filter=TlsTrustTest.IdentifiesAValidCertificate
```

Beide Werte müssen zeichengenau mit der openssl-Ausgabe übereinstimmen. **Danach die beiden `std::cout`-Zeilen und den Include wieder entfernen** — Testausgabe soll sauber bleiben. Die Werte gehören in den Bericht, nicht in den Quelltext.

- [ ] **Step 10: Committen**

```powershell
git add src/core tests/core tests/CMakeLists.txt
git commit -m "feat(core): TlsTrust berechnet Fingerabdruck und Public-Key-Pin"
```

---

## Task 3: CredentialStore portieren und die UTF-8-Wandlung reparieren

**Files:**
- Create: `src/core/util/CredentialStore.h`
- Create: `src/core/util/CredentialStore.cpp`
- Modify: `src/core/CMakeLists.txt`
- Test: `tests/core/credential_store_test.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: nichts aus früheren Tasks dieses Plans
- Produces: Klasse `baludesk::core::CredentialStore` mit den Statiken
  `bool save(const std::string& key, const std::string& secret)`,
  `std::optional<std::string> load(const std::string& key)`,
  `bool erase(const std::string& key)`,
  `bool contains(const std::string& key)`

---

- [ ] **Step 1: Den fehlschlagenden Test schreiben**

Diese Tests fassen den **echten** Windows Credential Manager an. Deshalb benutzen sie einen eigenen Namensraum mit einem Präfix, das sonst niemand verwendet, und räumen hinter sich auf.

Der Alt-Client wandelte `std::string` nach `std::wstring` mit `std::wstring(s.begin(), s.end())`. Das verbreitert jedes Byte einzeln und zerlegt jedes Mehrbyte-Zeichen in Unsinn. Der Test mit Umlauten fängt genau das.

Datei `tests/core/credential_store_test.cpp`:

```cpp
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
    ASSERT_TRUE(CredentialStore::save(kKeyAscii, "wird-geloescht"));
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
```

- [ ] **Step 2: `src/core/CMakeLists.txt` erweitern**

`util/CredentialStore.cpp` in die Quelldateiliste aufnehmen. Der Windows-Teil braucht `Advapi32` — das wird verknüpft statt über `#pragma comment(lib, ...)` eingezogen, damit die Abhängigkeit im Bausystem sichtbar ist:

```cmake
add_library(baludesk_core STATIC
    util/AppConfig.cpp
    util/Logger.cpp
    util/TlsTrust.cpp
    util/CredentialStore.cpp
)

target_include_directories(baludesk_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(baludesk_core PUBLIC
    spdlog::spdlog
    OpenSSL::Crypto
)

if(WIN32)
    target_link_libraries(baludesk_core PRIVATE Advapi32)
endif()
```

- [ ] **Step 3: `tests/CMakeLists.txt` erweitern**

```cmake
add_executable(baludesk_core_tests
    core/app_config_test.cpp
    core/logger_test.cpp
    core/tls_trust_test.cpp
    core/credential_store_test.cpp
)
```

- [ ] **Step 4: Test laufen lassen und Fehlschlag bestätigen**

```powershell
cmake --preset msvc2022
```

Erwartet: FEHLER beim Konfigurieren — `Cannot find source file: util/CredentialStore.cpp`.

- [ ] **Step 5: `src/core/util/CredentialStore.h` schreiben**

```cpp
#pragma once

#include <optional>
#include <string>

namespace baludesk::core {

// Ablage für Geheimnisse im Anmeldeinformationsspeicher des Betriebssystems.
//
// Windows: Credential Manager (wincred). Die Linux- und macOS-Zweige folgen,
// wenn diese Plattformen an der Reihe sind; bis dahin sind die Aufrufe dort
// nicht übersetzbar und das Ziel wird ohnehin nicht gebaut.
//
// Der Schlüssel ist ein freier Bezeichner wie "server/access-token". Er wird
// mit einem festen Präfix versehen, damit BaluDesks Einträge im Credential
// Manager erkennbar sind und nicht mit fremden kollidieren.
//
// Alle Zeichenketten sind UTF-8, in Schlüsseln wie in Werten. Werte dürfen
// Nullbytes enthalten.
class CredentialStore {
public:
    // Legt das Geheimnis ab und überschreibt einen vorhandenen Eintrag.
    // Ein leerer Schlüssel wird abgelehnt.
    static bool save(const std::string& key, const std::string& secret);

    // Liest das Geheimnis. Nichts, wenn kein Eintrag existiert.
    [[nodiscard]] static std::optional<std::string> load(const std::string& key);

    // Entfernt den Eintrag. Ein nicht vorhandener Eintrag gilt als Erfolg -
    // das Ziel ist erreicht.
    static bool erase(const std::string& key);

    [[nodiscard]] static bool contains(const std::string& key);
};

}  // namespace baludesk::core
```

- [ ] **Step 6: `src/core/util/CredentialStore.cpp` schreiben**

Der wesentliche Unterschied zum Alt-Client steckt in `toWide()`: `MultiByteToWideChar` mit `CP_UTF8` statt der byteweisen Verbreiterung.

```cpp
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
```

- [ ] **Step 7: Tests laufen lassen und grün bestätigen**

```powershell
cmake --preset msvc2022
cmake --build --preset debug
ctest --preset debug
```

Erwartet: 42 Tests, alle bestanden.

- [ ] **Step 8: Nachweisen, dass nichts im Credential Manager liegen bleibt**

```powershell
cmdkey /list | Select-String "BaluDesk"
```

Erwartet: **keine Ausgabe.** Die Tests räumen in `TearDown` auf. Bleibt etwas stehen, ist das ein Fehler im Aufräumen und zu melden — ein Testlauf darf keine Spuren im Anmeldeinformationsspeicher des Nutzers hinterlassen.

- [ ] **Step 9: Committen**

```powershell
git add src/core tests/core tests/CMakeLists.txt
git commit -m "feat(core): CredentialStore mit korrekter UTF-8-Wandlung"
```

---

## Task 4: HttpClient mit Transport-Naht und TLS-Politik

**Files:**
- Create: `src/core/api/HttpTypes.h`
- Create: `src/core/api/TlsPolicy.h`
- Create: `src/core/api/Transport.h`
- Create: `src/core/api/HttpClient.h`
- Create: `src/core/api/HttpClient.cpp`
- Create: `src/core/api/CurlTransport.h`
- Create: `src/core/api/CurlTransport.cpp`
- Modify: `src/core/CMakeLists.txt`
- Test: `tests/core/http_client_test.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `baludesk::core::CertificateIdentity` und `identifyCertificate` aus Task 2; `baludesk::core::Logger` aus Stufe 0
- Produces:
  - `enum class HttpMethod { Get, Post }`
  - `struct HttpRequest { HttpMethod method; std::string path; std::string jsonBody; std::string bearerToken; }`
  - `struct HttpResponse { long statusCode; std::string body; }`
  - `enum class HttpErrorKind { None, Network, CertificateUntrusted, CertificatePinMismatch, Timeout, Protocol }`
  - `struct TransportResult { HttpErrorKind error; HttpResponse response; std::string detail; std::string peerCertificatePem; }`
  - `struct TlsPolicy { enum class Mode { SystemTrust, Pinned }; Mode mode; std::string publicKeyPin; }`
  - `class Transport` mit `virtual TransportResult send(const std::string& url, const HttpRequest&, const TlsPolicy&) = 0`
  - `class CurlTransport : public Transport`
  - `class HttpClient` mit Konstruktor `HttpClient(std::string baseUrl, std::shared_ptr<Transport>, TlsPolicy)` und den Methoden `HttpResult get(const std::string& path, const std::string& bearerToken = {})`, `HttpResult postJson(const std::string& path, const std::string& jsonBody, const std::string& bearerToken = {})`, `const std::string& baseUrl() const`, `void setTlsPolicy(TlsPolicy)`
  - `struct HttpResult { HttpErrorKind error; long statusCode; std::string body; std::string detail; std::string peerCertificatePem; bool ok() const; }`

---

- [ ] **Step 1: Den fehlschlagenden Test schreiben**

Getestet wird alles, was ohne Netz prüfbar ist: Zusammensetzen der URL, Kopfzeilen, Weitergabe der TLS-Politik, Deutung der Antwort. Der echte curl-Aufruf steckt in `CurlTransport` und wird in Step 9 von Hand abgenommen.

Datei `tests/core/http_client_test.cpp`:

```cpp
#include "core/api/HttpClient.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using baludesk::core::HttpClient;
using baludesk::core::HttpErrorKind;
using baludesk::core::HttpMethod;
using baludesk::core::HttpRequest;
using baludesk::core::TlsPolicy;
using baludesk::core::Transport;
using baludesk::core::TransportResult;

namespace {

// Merkt sich, womit er aufgerufen wurde, und liefert eine vorgegebene Antwort.
class FakeTransport : public Transport {
public:
    struct Call {
        std::string url;
        HttpRequest request;
        TlsPolicy policy;
    };

    std::vector<Call> calls;
    TransportResult nextResult;

    TransportResult send(const std::string& url,
                         const HttpRequest& request,
                         const TlsPolicy& policy) override {
        calls.push_back(Call{url, request, policy});
        return nextResult;
    }
};

TransportResult okWith(long statusCode, std::string body) {
    TransportResult result;
    result.error = HttpErrorKind::None;
    result.response.statusCode = statusCode;
    result.response.body = std::move(body);
    return result;
}

TlsPolicy systemTrust() {
    TlsPolicy policy;
    policy.mode = TlsPolicy::Mode::SystemTrust;
    return policy;
}

}  // namespace

TEST(HttpClientTest, JoinsBaseUrlAndPathWithASingleSlash) {
    auto transport = std::make_shared<FakeTransport>();
    transport->nextResult = okWith(200, "{}");

    HttpClient client("https://nas.example.com", transport, systemTrust());
    client.get("/api/health");

    ASSERT_EQ(transport->calls.size(), std::size_t{1});
    EXPECT_EQ(transport->calls[0].url, "https://nas.example.com/api/health");
}

TEST(HttpClientTest, TolerationOfTrailingAndMissingSlashes) {
    auto transport = std::make_shared<FakeTransport>();
    transport->nextResult = okWith(200, "{}");

    HttpClient withSlash("https://nas.example.com/", transport, systemTrust());
    withSlash.get("/api/health");
    HttpClient withoutLeading("https://nas.example.com/", transport, systemTrust());
    withoutLeading.get("api/health");

    ASSERT_EQ(transport->calls.size(), std::size_t{2});
    EXPECT_EQ(transport->calls[0].url, "https://nas.example.com/api/health");
    EXPECT_EQ(transport->calls[1].url, "https://nas.example.com/api/health");
}

TEST(HttpClientTest, PassesTheBearerTokenThrough) {
    auto transport = std::make_shared<FakeTransport>();
    transport->nextResult = okWith(200, "{}");

    HttpClient client("https://nas.example.com", transport, systemTrust());
    client.get("/api/auth/me", "das-token");

    ASSERT_EQ(transport->calls.size(), std::size_t{1});
    EXPECT_EQ(transport->calls[0].request.bearerToken, "das-token");
    EXPECT_EQ(transport->calls[0].request.method, HttpMethod::Get);
}

TEST(HttpClientTest, SendsPostBodiesUnchanged) {
    auto transport = std::make_shared<FakeTransport>();
    transport->nextResult = okWith(201, "{}");

    HttpClient client("https://nas.example.com", transport, systemTrust());
    client.postJson("/api/desktop-pairing/poll", R"({"device_code":"abc"})");

    ASSERT_EQ(transport->calls.size(), std::size_t{1});
    EXPECT_EQ(transport->calls[0].request.method, HttpMethod::Post);
    EXPECT_EQ(transport->calls[0].request.jsonBody, R"({"device_code":"abc"})");
}

TEST(HttpClientTest, HandsTheTlsPolicyToTheTransport) {
    auto transport = std::make_shared<FakeTransport>();
    transport->nextResult = okWith(200, "{}");

    TlsPolicy pinned;
    pinned.mode = TlsPolicy::Mode::Pinned;
    pinned.publicKeyPin = "sha256//AAAA";

    HttpClient client("https://nas.example.com", transport, pinned);
    client.get("/api/health");

    ASSERT_EQ(transport->calls.size(), std::size_t{1});
    EXPECT_EQ(transport->calls[0].policy.mode, TlsPolicy::Mode::Pinned);
    EXPECT_EQ(transport->calls[0].policy.publicKeyPin, "sha256//AAAA");
}

TEST(HttpClientTest, AppliesAChangedTlsPolicyToLaterCalls) {
    auto transport = std::make_shared<FakeTransport>();
    transport->nextResult = okWith(200, "{}");

    HttpClient client("https://nas.example.com", transport, systemTrust());
    client.get("/api/health");

    TlsPolicy pinned;
    pinned.mode = TlsPolicy::Mode::Pinned;
    pinned.publicKeyPin = "sha256//BBBB";
    client.setTlsPolicy(pinned);
    client.get("/api/health");

    ASSERT_EQ(transport->calls.size(), std::size_t{2});
    EXPECT_EQ(transport->calls[0].policy.mode, TlsPolicy::Mode::SystemTrust);
    EXPECT_EQ(transport->calls[1].policy.mode, TlsPolicy::Mode::Pinned);
}

TEST(HttpClientTest, TreatsTwoHundredAsSuccess) {
    auto transport = std::make_shared<FakeTransport>();
    transport->nextResult = okWith(200, R"({"status":"ok"})");

    HttpClient client("https://nas.example.com", transport, systemTrust());
    const auto result = client.get("/api/health");

    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.statusCode, 200);
    EXPECT_EQ(result.body, R"({"status":"ok"})");
    EXPECT_EQ(result.error, HttpErrorKind::None);
}

TEST(HttpClientTest, DoesNotTreatFourHundredAsSuccess) {
    auto transport = std::make_shared<FakeTransport>();
    transport->nextResult = okWith(401, R"({"detail":"Not authenticated"})");

    HttpClient client("https://nas.example.com", transport, systemTrust());
    const auto result = client.get("/api/auth/me");

    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.statusCode, 401);
    // Der Rumpf bleibt erhalten - die Fehlermeldung des Servers ist die
    // brauchbarste Diagnose, die es gibt.
    EXPECT_EQ(result.body, R"({"detail":"Not authenticated"})");
}

TEST(HttpClientTest, CarriesTransportErrorsOutward) {
    auto transport = std::make_shared<FakeTransport>();
    TransportResult failure;
    failure.error = HttpErrorKind::CertificateUntrusted;
    failure.detail = "self signed certificate";
    failure.peerCertificatePem = "-----BEGIN CERTIFICATE-----\nXYZ\n-----END CERTIFICATE-----\n";
    transport->nextResult = failure;

    HttpClient client("https://nas.example.com", transport, systemTrust());
    const auto result = client.get("/api/health");

    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error, HttpErrorKind::CertificateUntrusted);
    EXPECT_EQ(result.detail, "self signed certificate");
    // Das Zertifikat der Gegenstelle muss durchgereicht werden - ohne es kann
    // der Nutzer den Fingerabdruck nicht bestätigen.
    EXPECT_FALSE(result.peerCertificatePem.empty());
}

TEST(HttpClientTest, ReportsAPinMismatchDistinctly) {
    auto transport = std::make_shared<FakeTransport>();
    TransportResult failure;
    failure.error = HttpErrorKind::CertificatePinMismatch;
    failure.detail = "pinned public key mismatch";
    transport->nextResult = failure;

    HttpClient client("https://nas.example.com", transport, systemTrust());
    const auto result = client.get("/api/health");

    // Eigene Fehlerart, weil daraus etwas anderes folgt als aus einem
    // unbekannten Zertifikat: hier wird nicht nachgefragt, sondern hart
    // abgebrochen.
    EXPECT_EQ(result.error, HttpErrorKind::CertificatePinMismatch);
}

TEST(HttpClientTest, ExposesItsBaseUrl) {
    auto transport = std::make_shared<FakeTransport>();
    HttpClient client("https://nas.example.com/", transport, systemTrust());
    EXPECT_EQ(client.baseUrl(), "https://nas.example.com");
}
```

- [ ] **Step 2: Test laufen lassen und Fehlschlag bestätigen**

Zuerst `tests/CMakeLists.txt` erweitern:

```cmake
add_executable(baludesk_core_tests
    core/app_config_test.cpp
    core/logger_test.cpp
    core/tls_trust_test.cpp
    core/credential_store_test.cpp
    core/http_client_test.cpp
)
```

dann

```powershell
cmake --preset msvc2022
cmake --build --preset debug
```

Erwartet: FEHLER beim Übersetzen — `Cannot open include file: 'core/api/HttpClient.h'`. Anders als in den vorigen Tasks bricht hier nicht schon CMake ab, weil die neue Quelldatei erst in Step 6 in `add_library` aufgenommen wird.

- [ ] **Step 3: `src/core/api/HttpTypes.h` schreiben**

```cpp
#pragma once

#include <string>

namespace baludesk::core {

enum class HttpMethod { Get, Post };

struct HttpRequest {
    HttpMethod method = HttpMethod::Get;

    // Pfad ab der Basis-URL, mit oder ohne führenden Schrägstrich.
    std::string path;

    // Nur bei Post ausgewertet. Wird unverändert gesendet, mit
    // Content-Type: application/json.
    std::string jsonBody;

    // Leer lassen, wenn die Anfrage ohne Anmeldung geht.
    std::string bearerToken;
};

struct HttpResponse {
    long statusCode = 0;
    std::string body;
};

// Warum eine Anfrage gescheitert ist. Absichtlich grob: der Client muss
// unterscheiden können, was daraus folgt, nicht jeden curl-Code kennen.
enum class HttpErrorKind {
    None,
    // Nicht erreichbar, DNS, Verbindung abgewiesen, Verbindung abgerissen.
    Network,
    // Zertifikat nicht vertrauenswürdig - selbstsigniert, falscher Name,
    // abgelaufen. Führt zur Nachfrage beim Nutzer.
    CertificateUntrusted,
    // Angehefteter Schlüssel passt nicht. Führt NICHT zur Nachfrage, sondern
    // zum harten Abbruch (Spec Abschnitt 2.9).
    CertificatePinMismatch,
    Timeout,
    // Antwort unverständlich, HTTP-Ebene kaputt.
    Protocol
};

struct TransportResult {
    HttpErrorKind error = HttpErrorKind::None;
    HttpResponse response;

    // Klartext für Protokoll und Anzeige, etwa curls Fehlermeldung.
    std::string detail;

    // Zertifikat der Gegenstelle im PEM-Format, wenn es zu ermitteln war.
    // Wird bei CertificateUntrusted gebraucht, damit der Nutzer den
    // Fingerabdruck bestätigen kann.
    std::string peerCertificatePem;
};

}  // namespace baludesk::core
```

- [ ] **Step 4: `src/core/api/TlsPolicy.h` schreiben**

```cpp
#pragma once

#include <string>

namespace baludesk::core {

// Wie eine Verbindung geprüft wird. Siehe Spec Abschnitt 2.9.
struct TlsPolicy {
    enum class Mode {
        // Normalfall: Prüfung gegen den Zertifikatsspeicher des Systems.
        SystemTrust,
        // Der Nutzer hat dieses eine Zertifikat beim Pairing bestätigt.
        // Geprüft wird dann gegen den angehefteten öffentlichen Schlüssel.
        Pinned
    };

    Mode mode = Mode::SystemTrust;

    // Nur bei Mode::Pinned belegt, im Format "sha256//<base64>" -
    // siehe CertificateIdentity::publicKeyPin in core/util/TlsTrust.h.
    std::string publicKeyPin;
};

}  // namespace baludesk::core
```

- [ ] **Step 5: `src/core/api/Transport.h` schreiben**

```cpp
#pragma once

#include <string>

#include "core/api/HttpTypes.h"
#include "core/api/TlsPolicy.h"

namespace baludesk::core {

// Die Naht, an der die Netzwerkschicht austauschbar wird.
//
// Alles Prüfbare - Pfadzusammensetzung, Kopfzeilen, Auswertung der Antwort -
// liegt in HttpClient darüber und ist gegen einen Fake testbar. Darunter
// bleibt nur der eigentliche curl-Aufruf, der von Hand abgenommen wird.
class Transport {
public:
    virtual ~Transport() = default;

    // Führt die Anfrage aus. Wirft nicht; Fehler kommen als HttpErrorKind
    // zurück.
    virtual TransportResult send(const std::string& url,
                                 const HttpRequest& request,
                                 const TlsPolicy& policy) = 0;
};

}  // namespace baludesk::core
```

- [ ] **Step 6: `src/core/api/HttpClient.h` und `.cpp` schreiben**

`src/core/api/HttpClient.h`:

```cpp
#pragma once

#include <memory>
#include <string>

#include "core/api/HttpTypes.h"
#include "core/api/TlsPolicy.h"
#include "core/api/Transport.h"

namespace baludesk::core {

struct HttpResult {
    HttpErrorKind error = HttpErrorKind::None;
    long statusCode = 0;
    std::string body;
    std::string detail;
    std::string peerCertificatePem;

    // Erfolgreich heißt: kein Transportfehler und ein 2xx-Status.
    [[nodiscard]] bool ok() const {
        return error == HttpErrorKind::None && statusCode >= 200 && statusCode < 300;
    }
};

// Setzt Anfragen gegen einen Server zusammen und deutet die Antworten.
// Kennt weder curl noch Qt.
class HttpClient {
public:
    HttpClient(std::string baseUrl, std::shared_ptr<Transport> transport, TlsPolicy policy);

    HttpResult get(const std::string& path, const std::string& bearerToken = {});
    HttpResult postJson(const std::string& path,
                        const std::string& jsonBody,
                        const std::string& bearerToken = {});

    // Ohne abschließenden Schrägstrich.
    [[nodiscard]] const std::string& baseUrl() const { return baseUrl_; }

    // Wird gesetzt, sobald der Nutzer ein Zertifikat bestätigt hat.
    void setTlsPolicy(TlsPolicy policy) { policy_ = std::move(policy); }

private:
    HttpResult send(HttpRequest request);
    [[nodiscard]] std::string urlFor(const std::string& path) const;

    std::string baseUrl_;
    std::shared_ptr<Transport> transport_;
    TlsPolicy policy_;
};

}  // namespace baludesk::core
```

`src/core/api/HttpClient.cpp`:

```cpp
#include "core/api/HttpClient.h"

#include <utility>

#include "core/util/Logger.h"

namespace baludesk::core {
namespace {

std::string withoutTrailingSlash(std::string url) {
    while (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
    return url;
}

}  // namespace

HttpClient::HttpClient(std::string baseUrl,
                       std::shared_ptr<Transport> transport,
                       TlsPolicy policy)
    : baseUrl_(withoutTrailingSlash(std::move(baseUrl))),
      transport_(std::move(transport)),
      policy_(std::move(policy)) {}

std::string HttpClient::urlFor(const std::string& path) const {
    if (path.empty()) {
        return baseUrl_;
    }
    if (path.front() == '/') {
        return baseUrl_ + path;
    }
    return baseUrl_ + "/" + path;
}

HttpResult HttpClient::send(HttpRequest request) {
    const std::string url = urlFor(request.path);

    const TransportResult transportResult = transport_->send(url, request, policy_);

    HttpResult result;
    result.error = transportResult.error;
    result.statusCode = transportResult.response.statusCode;
    result.body = transportResult.response.body;
    result.detail = transportResult.detail;
    result.peerCertificatePem = transportResult.peerCertificatePem;

    if (result.error != HttpErrorKind::None) {
        Logger::warn("HTTP: {} fehlgeschlagen: {}", url, result.detail);
    } else if (!result.ok()) {
        Logger::warn("HTTP: {} antwortete mit Status {}", url, result.statusCode);
    }

    return result;
}

HttpResult HttpClient::get(const std::string& path, const std::string& bearerToken) {
    HttpRequest request;
    request.method = HttpMethod::Get;
    request.path = path;
    request.bearerToken = bearerToken;
    return send(std::move(request));
}

HttpResult HttpClient::postJson(const std::string& path,
                                const std::string& jsonBody,
                                const std::string& bearerToken) {
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.path = path;
    request.jsonBody = jsonBody;
    request.bearerToken = bearerToken;
    return send(std::move(request));
}

}  // namespace baludesk::core
```

`src/core/CMakeLists.txt` erweitern — **`api/CurlTransport.cpp` noch nicht**, die Datei entsteht erst in Step 8:

```cmake
add_library(baludesk_core STATIC
    util/AppConfig.cpp
    util/Logger.cpp
    util/TlsTrust.cpp
    util/CredentialStore.cpp
    api/HttpClient.cpp
)
```

- [ ] **Step 7: Tests laufen lassen und grün bestätigen**

Die Tests kommen ohne `CurlTransport` aus — sie laufen gegen den Fake. Genau das ist der Zweck der Naht.

```powershell
cmake --preset msvc2022
cmake --build --preset debug
ctest --preset debug
```

Erwartet: 53 Tests, alle bestanden.

- [ ] **Step 8: `src/core/api/CurlTransport.h` und `.cpp` schreiben**

Die drei tragenden Entscheidungen stehen als Kommentar im Code, weil sie sonst beim nächsten Anfassen verloren gehen.

`src/core/api/CurlTransport.h`:

```cpp
#pragma once

#include <string>

#include "core/api/Transport.h"

namespace baludesk::core {

// libcurl hinter der Transport-Naht.
//
// Nicht unit-getestet: was hier passieren kann, passiert im Netz. Geprüft
// wird diese Klasse durch die Abnahme gegen einen echten Server.
//
// curl_global_init/cleanup werden vom Konstruktor und Destruktor gehalten.
// Es darf deshalb nur eine Instanz je Programmlauf geben.
class CurlTransport : public Transport {
public:
    CurlTransport();
    ~CurlTransport() override;

    CurlTransport(const CurlTransport&) = delete;
    CurlTransport& operator=(const CurlTransport&) = delete;

    TransportResult send(const std::string& url,
                         const HttpRequest& request,
                         const TlsPolicy& policy) override;
};

}  // namespace baludesk::core
```

`src/core/api/CurlTransport.cpp`:

```cpp
#include "core/api/CurlTransport.h"

#include <curl/curl.h>

#include <memory>
#include <string>

#include "core/util/Logger.h"

namespace baludesk::core {
namespace {

constexpr long kConnectTimeoutSeconds = 10;
constexpr long kRequestTimeoutSeconds = 30;

std::size_t appendToString(char* data, std::size_t size, std::size_t count, void* userData) {
    auto* target = static_cast<std::string*>(userData);
    const std::size_t total = size * count;
    target->append(data, total);
    return total;
}

struct CurlDeleter {
    void operator()(CURL* handle) const noexcept { curl_easy_cleanup(handle); }
};
struct SlistDeleter {
    void operator()(curl_slist* list) const noexcept { curl_slist_free_all(list); }
};

// Das Zertifikat der Gegenstelle aus CURLINFO_CERTINFO ziehen. Nur nötig,
// wenn die Prüfung fehlgeschlagen ist und der Nutzer gefragt werden soll.
std::string peerCertificatePem(CURL* handle) {
    curl_certinfo* info = nullptr;
    if (curl_easy_getinfo(handle, CURLINFO_CERTINFO, &info) != CURLE_OK || info == nullptr) {
        return {};
    }
    if (info->num_of_certs <= 0) {
        return {};
    }

    // Das erste Zertifikat der Kette ist das des Servers.
    for (curl_slist* entry = info->certinfo[0]; entry != nullptr; entry = entry->next) {
        const std::string field = entry->data;
        const std::string prefix = "Cert:";
        if (field.rfind(prefix, 0) == 0) {
            return field.substr(prefix.size());
        }
    }
    return {};
}

HttpErrorKind classify(CURLcode code) {
    switch (code) {
        case CURLE_OK:
            return HttpErrorKind::None;
        case CURLE_PEER_FAILED_VERIFICATION:
        case CURLE_SSL_CACERT_BADFILE:
            return HttpErrorKind::CertificateUntrusted;
        case CURLE_SSL_PINNEDPUBKEYNOTMATCH:
            return HttpErrorKind::CertificatePinMismatch;
        case CURLE_OPERATION_TIMEDOUT:
            return HttpErrorKind::Timeout;
        case CURLE_COULDNT_RESOLVE_HOST:
        case CURLE_COULDNT_RESOLVE_PROXY:
        case CURLE_COULDNT_CONNECT:
        case CURLE_SEND_ERROR:
        case CURLE_RECV_ERROR:
        case CURLE_GOT_NOTHING:
            return HttpErrorKind::Network;
        default:
            return HttpErrorKind::Protocol;
    }
}

}  // namespace

CurlTransport::CurlTransport() {
    const CURLcode code = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (code != CURLE_OK) {
        Logger::error("curl_global_init fehlgeschlagen: {}", curl_easy_strerror(code));
    }
}

CurlTransport::~CurlTransport() {
    curl_global_cleanup();
}

TransportResult CurlTransport::send(const std::string& url,
                                    const HttpRequest& request,
                                    const TlsPolicy& policy) {
    TransportResult result;

    std::unique_ptr<CURL, CurlDeleter> handle(curl_easy_init());
    if (!handle) {
        result.error = HttpErrorKind::Network;
        result.detail = "curl_easy_init fehlgeschlagen";
        return result;
    }

    std::string responseBody;
    char errorBuffer[CURL_ERROR_SIZE] = {};

    curl_easy_setopt(handle.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle.get(), CURLOPT_ERRORBUFFER, errorBuffer);
    curl_easy_setopt(handle.get(), CURLOPT_WRITEFUNCTION, appendToString);
    curl_easy_setopt(handle.get(), CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(handle.get(), CURLOPT_CONNECTTIMEOUT, kConnectTimeoutSeconds);
    curl_easy_setopt(handle.get(), CURLOPT_TIMEOUT, kRequestTimeoutSeconds);
    curl_easy_setopt(handle.get(), CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(handle.get(), CURLOPT_USERAGENT, "BaluDesk/0.1.0");
    curl_easy_setopt(handle.get(), CURLOPT_NOSIGNAL, 1L);
    // Immer anfordern: bei einem Zertifikatsfehler ist das die einzige Quelle
    // für den Fingerabdruck, den der Nutzer bestätigen soll.
    curl_easy_setopt(handle.get(), CURLOPT_CERTINFO, 1L);

    // TLS. curl ist gegen OpenSSL gebaut; CURLSSLOPT_NATIVE_CA lässt trotzdem
    // den Windows-Wurzelspeicher gelten, damit ein Let's-Encrypt-Zertifikat
    // ohne mitgelieferte CA-Sammlung anerkannt wird.
    curl_easy_setopt(handle.get(), CURLOPT_SSL_OPTIONS, static_cast<long>(CURLSSLOPT_NATIVE_CA));

    if (policy.mode == TlsPolicy::Mode::Pinned && !policy.publicKeyPin.empty()) {
        // Der Nutzer hat dieses Zertifikat bestätigt. Die Kettenprüfung kann
        // nicht gelingen - sonst wäre es nie zur Nachfrage gekommen -, also
        // wird sie abgeschaltet und durch den Pin ersetzt. curl prüft den Pin
        // beim Handschlag, bevor irgendein Byte des Rumpfes gesendet wird.
        curl_easy_setopt(handle.get(), CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(handle.get(), CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(handle.get(), CURLOPT_PINNEDPUBLICKEY, policy.publicKeyPin.c_str());
    } else {
        curl_easy_setopt(handle.get(), CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(handle.get(), CURLOPT_SSL_VERIFYHOST, 2L);
    }

    std::unique_ptr<curl_slist, SlistDeleter> headers;
    curl_slist* rawHeaders = nullptr;
    rawHeaders = curl_slist_append(rawHeaders, "Accept: application/json");
    if (request.method == HttpMethod::Post) {
        rawHeaders = curl_slist_append(rawHeaders, "Content-Type: application/json");
    }
    std::string authorization;
    if (!request.bearerToken.empty()) {
        authorization = "Authorization: Bearer " + request.bearerToken;
        rawHeaders = curl_slist_append(rawHeaders, authorization.c_str());
    }
    headers.reset(rawHeaders);
    curl_easy_setopt(handle.get(), CURLOPT_HTTPHEADER, headers.get());

    if (request.method == HttpMethod::Post) {
        curl_easy_setopt(handle.get(), CURLOPT_POST, 1L);
        curl_easy_setopt(handle.get(), CURLOPT_POSTFIELDS, request.jsonBody.c_str());
        curl_easy_setopt(handle.get(), CURLOPT_POSTFIELDSIZE,
                         static_cast<long>(request.jsonBody.size()));
    }

    const CURLcode code = curl_easy_perform(handle.get());

    result.error = classify(code);
    if (code != CURLE_OK) {
        result.detail = errorBuffer[0] != '\0' ? errorBuffer : curl_easy_strerror(code);
        if (result.error == HttpErrorKind::CertificateUntrusted) {
            result.peerCertificatePem = peerCertificatePem(handle.get());
        }
        return result;
    }

    long statusCode = 0;
    curl_easy_getinfo(handle.get(), CURLINFO_RESPONSE_CODE, &statusCode);
    result.response.statusCode = statusCode;
    result.response.body = std::move(responseBody);
    return result;
}

}  // namespace baludesk::core
```

Jetzt kommt die Datei ins Bausystem und curl wird verknüpft — `src/core/CMakeLists.txt`:

```cmake
find_package(spdlog CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)

add_library(baludesk_core STATIC
    util/AppConfig.cpp
    util/Logger.cpp
    util/TlsTrust.cpp
    util/CredentialStore.cpp
    api/HttpClient.cpp
    api/CurlTransport.cpp
)

target_include_directories(baludesk_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(baludesk_core PUBLIC
    spdlog::spdlog
    OpenSSL::Crypto
    CURL::libcurl
)

if(WIN32)
    target_link_libraries(baludesk_core PRIVATE Advapi32)
endif()
```

Danach neu bauen und die Testsuite gegenprüfen — sie muss unverändert 53 grün melden:

```powershell
cmake --preset msvc2022
cmake --build --preset debug
ctest --preset debug
```

- [ ] **Step 9: Abnahme gegen den echten Server**

Das ist der Punkt, an dem sich zeigt, ob die TLS-Politik trägt. Ohne diesen Schritt ist Task 4 nicht fertig.

Lege ein kleines Abnahmeprogramm an, `tools/probe/main.cpp`, und binde es über `tools/probe/CMakeLists.txt` als Ziel `baludesk_probe` ein (`add_executable(baludesk_probe main.cpp)`, `target_link_libraries(baludesk_probe PRIVATE baludesk_core)`), dazu `add_subdirectory(tools/probe)` in der Wurzel-`CMakeLists.txt` **vor** dem Absicherungsblock:

```cpp
#include <iostream>
#include <memory>
#include <string>

#include "core/api/CurlTransport.h"
#include "core/api/HttpClient.h"
#include "core/util/Logger.h"
#include "core/util/TlsTrust.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Aufruf: baludesk_probe <server-url> [pin]\n";
        return 2;
    }

    baludesk::core::TlsPolicy policy;
    if (argc >= 3) {
        policy.mode = baludesk::core::TlsPolicy::Mode::Pinned;
        policy.publicKeyPin = argv[2];
    }

    auto transport = std::make_shared<baludesk::core::CurlTransport>();
    baludesk::core::HttpClient client(argv[1], transport, policy);

    const auto result = client.get("/api/health");

    std::cout << "ok:     " << (result.ok() ? "ja" : "nein") << "\n";
    std::cout << "status: " << result.statusCode << "\n";
    std::cout << "fehler: " << static_cast<int>(result.error) << "\n";
    std::cout << "detail: " << result.detail << "\n";
    std::cout << "rumpf:  " << result.body << "\n";

    if (!result.peerCertificatePem.empty()) {
        const auto identity = baludesk::core::identifyCertificate(result.peerCertificatePem);
        if (identity) {
            std::cout << "fingerabdruck: " << identity->fingerprintSha256 << "\n";
            std::cout << "pin:           " << identity->publicKeyPin << "\n";
        } else {
            std::cout << "Zertifikat der Gegenstelle nicht lesbar\n";
        }
    }
    return result.ok() ? 0 : 1;
}
```

Dann drei Läufe, deren Ausgabe vollständig in den Bericht gehört:

**A — gegen einen Server mit gültigem Zertifikat** (falls vorhanden, sonst überspringen und im Bericht vermerken): `ok: ja`, Status 200.

**B — gegen den eigenen Server mit selbstsigniertem oder LAN-Zertifikat:** erwartet `ok: nein`, `fehler: 2` (`CertificateUntrusted`), und **Fingerabdruck und Pin müssen ausgegeben werden**. Werden sie es nicht, liefert `CURLINFO_CERTINFO` nichts und die ganze Bestätigungsmechanik der Stufe 1b hätte keine Grundlage — dann melden, nicht umgehen.

**C — derselbe Server, mit dem in B ausgegebenen Pin als zweitem Argument:** erwartet `ok: ja`, Status 200. Danach denselben Aufruf mit einem verfälschten Pin (ein Zeichen im Base64-Teil ändern) — erwartet `fehler: 3` (`CertificatePinMismatch`), **nicht** 2.

Lauf C ist der eigentliche Nachweis: er zeigt, dass Pinning greift und dass ein falscher Pin anders gemeldet wird als ein unbekanntes Zertifikat.

- [ ] **Step 10: Committen**

```powershell
git add src/core tests/core tests/CMakeLists.txt tools CMakeLists.txt
git commit -m "feat(core): HttpClient mit Transport-Naht und TLS-Politik"
```

---

## Abnahme Stufe 1a

Aus einem gelöschten Build-Verzeichnis:

```powershell
cd "F:\Programme (x86)\BaluDesk QT"
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
cmake --preset msvc2022
cmake --build --preset debug
ctest --preset debug
```

Alle fünf Punkte müssen zutreffen:

1. Konfigurieren läuft durch, `libcrypto-3-x64.dll` und `libssl-3-x64.dll` liegen unter `build/msvc2022/vcpkg_installed/x64-windows/bin` — curl spricht also OpenSSL
2. Bauen ohne Warnungen aus eigenem Code
3. `ctest --preset debug` meldet **53 bestandene Tests**, 0 fehlgeschlagen
4. `cmdkey /list | Select-String "BaluDesk"` gibt nichts aus — die Tests haben aufgeräumt
5. Die drei Läufe des Abnahmeprogramms aus Task 4 Step 9 verhalten sich wie beschrieben, insbesondere Lauf C mit korrektem und mit verfälschtem Pin

---

## Was Stufe 1b braucht und hier bewusst fehlt

- **Gerätecode-Ablauf** gegen `/api/desktop-pairing/device-code` und `/poll`. Das Poll-Ratenlimit ist 12 Anfragen je Minute bei einem vorgegebenen Intervall von 5 Sekunden — exakt ein Abruf je Intervall, ohne Reserve. Ein Client, der bei `authorization_pending` sofort erneut fragt, sperrt sich selbst aus.
- **Sitzungsspeicherung:** Zugangs- und Erneuerungstoken in den `CredentialStore`, Server-URL und bestätigter Pin in die Einstellungen. Der Pin ist kein Geheimnis.
- **`/api/auth/refresh` rotiert das Erneuerungstoken nicht.** Läuft es ab, hilft kein Erneuern mehr — dann muss neu gepaart werden. Der Ablauf muss diesen Fall vom gewöhnlichen 401 unterscheiden.
- **Dialoge:** Server-URL eingeben, Zertifikat bestätigen (Fingerabdruck aus `identifyCertificate`), Code anzeigen samt `verification_url`, Fortschritt, Abmelden.
- **Nicht vergessen:** Bei `CertificatePinMismatch` wird nicht nachgefragt, sondern hart abgebrochen (Spec Abschnitt 2.9). Die Fehlerart ist deshalb schon hier von `CertificateUntrusted` getrennt.
