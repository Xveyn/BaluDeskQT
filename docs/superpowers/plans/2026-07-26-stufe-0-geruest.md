# BaluDesk QT — Stufe 0 (Gerüst) Implementierungsplan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ein baubares, startendes Qt-Widgets-Programm mit Qt-freier `core/`-Bibliothek, rotierendem Protokoll, Einstellungsspeicher und grüner GTest-Suite — die Grundlage, auf der die Stufen 1 bis 7 aufsetzen.

**Architecture:** Ein CMake-Projekt mit drei Zielen: `baludesk_core` (statische Bibliothek, reines C++17, kennt Qt nicht), `baludesk_app_lib` (Qt-Widgets-Code als statische Bibliothek, damit Tests dagegen linken können) und `baludesk` (dünne `main.cpp`). Zwei getrennte Testbinaries erzwingen die Qt-Freiheit von `core/` sichtbar im Build: `baludesk_core_tests` linkt kein Qt, `baludesk_app_tests` schon.

**Tech Stack:** C++17 · CMake 4.2.1 mit CMakePresets · Visual Studio 2022 Build Tools (Toolset 17.14) · Qt 6.8.0 msvc2022_64 · vcpkg im Manifest-Modus (curl, sqlite3, nlohmann-json, spdlog, gtest) · GoogleTest

**Spec:** `docs/superpowers/specs/2026-07-26-baludesk-qt-design.md`, Abschnitt 12 Stufe 0

## Global Constraints

Diese Vorgaben gelten für jede Aufgabe in diesem Plan.

- **C++-Standard:** C++17, `CMAKE_CXX_EXTENSIONS OFF`
- **`cmake_minimum_required(VERSION 3.25)`** — nicht niedriger. CMake 4.x bricht bei Projekten ab, die eine Mindestversion unter 3.5 verlangen.
- **Qt:** exakt `F:/Qt/6.8.0/msvc2022_64`, Module Core, Gui, Widgets. Kein QML, kein Qt Quick.
- **vcpkg:** Installation unter `F:/vcpkg`, Triplet `x64-windows`, Manifest-Modus (`vcpkg.json` im Repo, kein Submodul)
- **`baludesk_core` darf niemals Qt linken.** Kein `find_package(Qt6)` in `src/core/CMakeLists.txt`, kein `QString`, kein `QObject`. Spec Abschnitt 2.4.
- **Namensräume:** `baludesk::core` für alles unter `src/core/`, `baludesk::app` für alles unter `src/app/`
- **Include-Pfade sind immer vollständig ab `src/`**, also `#include "core/util/Logger.h"`, nie relativ mit `../`
- **Sprache:** Bezeichner und Dateinamen englisch, Kommentare und Protokolltexte deutsch
- **Umlaute:** In allen Dateien echte UTF-8-Zeichen (`ü`, `ö`, `ä`, `ß`) — keine Behelfsschreibung wie `ue` oder `ss`. Das gilt für sichtbare Oberflächentexte, Protokolltexte, Kommentare und Dokumentation gleichermaßen. Quelldateien werden als UTF-8 ohne BOM gespeichert; `/utf-8` steht bereits in den Übersetzeroptionen, damit MSVC sie korrekt liest.
  **Einzige Ausnahme: Git-Commit-Nachrichten bleiben ASCII.** Sie werden unter Windows über PowerShell-5.1-Here-Strings an `git` übergeben, und deren Kodierung verstümmelt Umlaute zuverlässig. Dort also `Geruest` statt `Gerüst`.
- **Generator:** `Visual Studio 17 2022`, Architektur x64. Bewusst nicht Ninja, weil der VS-Generator MSVC selbst findet und keine `vcvars`-Umgebung braucht.
- **Warnungen:** `/W4 /permissive- /utf-8` unter MSVC
- **Nicht in Stufe 0:** Pairing, Sync, Datenbankschema, Infobereichssymbol, Dashboard, Explorer, Installer

---

## Dateistruktur nach Abschluss

```
BaluDesk QT/
├── .gitignore
├── CMakeLists.txt                 # Projektwurzel, bindet die drei Unterverzeichnisse ein
├── CMakePresets.json              # ein Configure-Preset, zwei Build-Presets, ein Test-Preset
├── README.md                      # Bauanleitung für einen frischen Rechner
├── vcpkg.json                     # Abhängigkeitsmanifest
├── src/
│   ├── app/
│   │   ├── CMakeLists.txt         # baludesk_app_lib + baludesk.exe
│   │   ├── main.cpp               # nur Einstiegspunkt, Verdrahtung
│   │   ├── MainWindow.h/.cpp      # leeres Hauptfenster
│   │   └── SettingsStore.h/.cpp   # QSettings ↔ core::AppConfig
│   └── core/
│       ├── CMakeLists.txt         # baludesk_core, ohne Qt
│       └── util/
│           ├── AppConfig.h/.cpp   # Einstellungswerte als schlichte Struktur + Bereichsprüfung
│           └── Logger.h/.cpp      # spdlog mit Rotation und Obergrenze
└── tests/
    ├── CMakeLists.txt             # zwei Testbinaries
    ├── core/
    │   ├── app_config_test.cpp
    │   └── logger_test.cpp
    └── app/
        └── settings_store_test.cpp
```

**Warum `baludesk_app_lib` und nicht alles in der .exe:** Ein Testbinary kann nicht gegen eine ausführbare Datei linken. Der gesamte App-Code liegt deshalb in einer statischen Bibliothek, `main.cpp` enthält nur den Einstiegspunkt.

---

## Task 1: Build-Gerüst mit startendem Fenster

**Files:**
- Create: `.gitignore`
- Create: `vcpkg.json`
- Create: `CMakeLists.txt`
- Create: `CMakePresets.json`
- Create: `src/app/CMakeLists.txt`
- Create: `src/app/main.cpp`
- Create: `src/app/MainWindow.h`
- Create: `src/app/MainWindow.cpp`
- Create: `README.md`
- Test: manueller Rauchtest (die .exe startet und zeigt ein Fenster)

**Interfaces:**
- Consumes: nichts (erste Aufgabe)
- Produces: CMake-Ziele `baludesk_app_lib` (STATIC) und `baludesk` (ausführbar); Klasse `baludesk::app::MainWindow` mit Konstruktor `explicit MainWindow(QWidget* parent = nullptr)`; Configure-Preset `msvc2022`, Build-Presets `debug`/`release`

---

- [ ] **Step 1: vcpkg bereitstellen**

Einmalige Einrichtung außerhalb des Repos. In PowerShell:

```powershell
git clone https://github.com/microsoft/vcpkg.git F:\vcpkg
F:\vcpkg\bootstrap-vcpkg.bat -disableMetrics
```

Prüfen, dass es läuft:

```powershell
F:\vcpkg\vcpkg.exe version
```

Erwartet: eine Versionszeile, kein Fehler.

*Hinweis:* Auf der Maschine existiert bereits ein gebootstrapptes vcpkg unter `F:\Programme (x86)\BaluDesk\vcpkg`. Es wird **nicht** verwendet — das neue Repo soll nicht am alten hängen (Spec Abschnitt 2.8).

- [ ] **Step 2: `.gitignore` anlegen**

```gitignore
# Build-Ausgaben
/build/
/out/

# vcpkg im Manifest-Modus
/vcpkg_installed/

# Persönliche CMake-Presets (Pfade, die nur lokal gelten)
CMakeUserPresets.json

# Qt Creator / Visual Studio
*.user
/.vs/
/.vscode/

# Laufzeitdaten
/logs/
*.log
```

- [ ] **Step 3: `vcpkg.json` anlegen**

Alle fünf Abhängigkeiten kommen sofort ins Manifest, obwohl Stufe 0 nur spdlog und gtest benutzt. Grund: der Zweck des Gerüsts ist zu beweisen, dass der komplette Abhängigkeitssatz auf dieser Maschine auflösbar ist. Ein Fehlschlag bei curl oder sqlite3 soll jetzt auffallen, nicht in Stufe 2.

```json
{
  "$schema": "https://raw.githubusercontent.com/microsoft/vcpkg-tool/main/docs/vcpkg.schema.json",
  "name": "baludesk-qt",
  "version-string": "0.1.0",
  "description": "Desktop-Synchronisationsclient für BaluHost NAS",
  "dependencies": [
    "curl",
    "sqlite3",
    "nlohmann-json",
    "spdlog",
    "gtest"
  ]
}
```

- [ ] **Step 4: Wurzel-`CMakeLists.txt` anlegen**

`src/core` und `tests` werden hier noch **nicht** eingebunden — die Verzeichnisse entstehen erst in Task 2.

```cmake
cmake_minimum_required(VERSION 3.25)

project(BaluDeskQT
    VERSION 0.1.0
    DESCRIPTION "Desktop-Synchronisationsclient für BaluHost NAS"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
endif()

enable_testing()

add_subdirectory(src/app)
```

- [ ] **Step 5: `CMakePresets.json` anlegen**

Ein Configure-Preset, weil der Visual-Studio-Generator mehrere Konfigurationen in einem Build-Verzeichnis hält. Debug und Release unterscheiden sich nur beim Bauen.

```json
{
  "version": 6,
  "cmakeMinimumRequired": { "major": 3, "minor": 25, "patch": 0 },
  "configurePresets": [
    {
      "name": "msvc2022",
      "displayName": "MSVC 2022 x64",
      "generator": "Visual Studio 17 2022",
      "architecture": "x64",
      "binaryDir": "${sourceDir}/build/msvc2022",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "F:/vcpkg/scripts/buildsystems/vcpkg.cmake",
        "VCPKG_TARGET_TRIPLET": "x64-windows",
        "CMAKE_PREFIX_PATH": "F:/Qt/6.8.0/msvc2022_64"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "debug",
      "displayName": "Debug bauen",
      "configurePreset": "msvc2022",
      "configuration": "Debug"
    },
    {
      "name": "release",
      "displayName": "Release bauen",
      "configurePreset": "msvc2022",
      "configuration": "Release"
    }
  ],
  "testPresets": [
    {
      "name": "debug",
      "displayName": "Tests (Debug)",
      "configurePreset": "msvc2022",
      "configuration": "Debug",
      "output": { "outputOnFailure": true }
    }
  ]
}
```

- [ ] **Step 6: `src/app/MainWindow.h` anlegen**

```cpp
#pragma once

#include <QMainWindow>

namespace baludesk::app {

// Hauptfenster der Anwendung. In Stufe 0 bewusst leer - es dient nur dem
// Nachweis, dass Qt korrekt eingebunden ist und die Anwendung startet.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
};

}  // namespace baludesk::app
```

- [ ] **Step 7: `src/app/MainWindow.cpp` anlegen**

```cpp
#include "app/MainWindow.h"

#include <QLabel>
#include <QCoreApplication>

namespace baludesk::app {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("BaluDesk %1").arg(QCoreApplication::applicationVersion()));
    resize(900, 600);

    auto* placeholder = new QLabel(
        QStringLiteral("BaluDesk QT — Gerüst steht.\nStufe 1 bringt das Geräte-Pairing."),
        this);
    placeholder->setAlignment(Qt::AlignCenter);
    setCentralWidget(placeholder);
}

}  // namespace baludesk::app
```

- [ ] **Step 8: `src/app/main.cpp` anlegen**

Noch ohne Protokoll und Einstellungen — die kommen in Task 4 dazu.

```cpp
#include <QApplication>

#include "app/MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral("BaluHost"));
    QCoreApplication::setApplicationName(QStringLiteral("BaluDesk"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    baludesk::app::MainWindow window;
    window.show();

    return application.exec();
}
```

- [ ] **Step 9: `src/app/CMakeLists.txt` anlegen**

`qt_standard_project_setup()` schaltet AUTOMOC ein, was `MainWindow` wegen `Q_OBJECT` braucht. Der `windeployqt`-Schritt nach dem Bauen kopiert die Qt-DLLs neben die .exe — ohne ihn startet sie nur aus einer Umgebung mit passendem `PATH`. Der Installer selbst ist Sache von Stufe 7.

```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets)

qt_standard_project_setup()

add_library(baludesk_app_lib STATIC
    MainWindow.cpp
)

target_include_directories(baludesk_app_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(baludesk_app_lib PUBLIC Qt6::Widgets)

qt_add_executable(baludesk
    main.cpp
)

target_link_libraries(baludesk PRIVATE baludesk_app_lib)

set_target_properties(baludesk PROPERTIES WIN32_EXECUTABLE ON)

# Qt-Laufzeitbibliotheken neben die ausführbare Datei kopieren, damit sie
# direkt aus dem Build-Verzeichnis startet.
if(WIN32)
    get_target_property(_qt_core_location Qt6::Core IMPORTED_LOCATION_RELEASE)
    if(NOT _qt_core_location)
        get_target_property(_qt_core_location Qt6::Core IMPORTED_LOCATION)
    endif()
    get_filename_component(_qt_bin_dir "${_qt_core_location}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt
        HINTS "${QT6_INSTALL_PREFIX}/bin" "${_qt_bin_dir}"
        REQUIRED
    )

    add_custom_command(TARGET baludesk POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                "$<TARGET_FILE:baludesk>"
        COMMENT "Qt-Laufzeitbibliotheken neben die .exe kopieren"
        VERBATIM
    )
endif()
```

- [ ] **Step 10: Konfigurieren**

```powershell
cd "F:\Programme (x86)\BaluDesk QT"
cmake --preset msvc2022
```

Erwartet: vcpkg baut beim ersten Mal curl, sqlite3, nlohmann-json, spdlog und gtest — das dauert mehrere Minuten. Danach `-- Configuring done` und `-- Generating done`.

Wenn vcpkg an einem Port scheitert, weil CMake 4.x dessen Mindestversion ablehnt: die Fehlermeldung nennt den Port. In dem Fall im Configure-Aufruf `-DVCPKG_MAX_CONCURRENCY=1` ergänzen, um die Ausgabe lesbar zu machen, und den Port-Fehler melden — nicht selbst am Port patchen.

- [ ] **Step 11: Bauen**

```powershell
cmake --build --preset debug
```

Erwartet: `baludesk.exe` unter `build/msvc2022/src/app/Debug/`, ohne Warnungen aus eigenem Code.

- [ ] **Step 12: Rauchtest — die Anwendung starten**

```powershell
& "build\msvc2022\src\app\Debug\baludesk.exe"
```

Erwartet: Ein Fenster 900×600 mit Titel „BaluDesk 0.1.0" und dem Platzhaltertext erscheint. Fenster schließen beendet den Prozess.

Startet die .exe nicht: prüfen, ob `windeployqt` die DLLs (`Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`, Ordner `platforms/`) neben die .exe gelegt hat.

- [ ] **Step 13: `README.md` anlegen**

````markdown
# BaluDesk QT

Desktop-Synchronisationsclient für BaluHost NAS. Qt 6 Widgets, C++17.

Entwurf und Begründung der Entscheidungen:
[`docs/superpowers/specs/2026-07-26-baludesk-qt-design.md`](docs/superpowers/specs/2026-07-26-baludesk-qt-design.md)

## Voraussetzungen

| Werkzeug | Version | Erwarteter Pfad |
|---|---|---|
| Qt | 6.8.0, Kit `msvc2022_64` | `F:\Qt\6.8.0\msvc2022_64` |
| Visual Studio Build Tools | 2022 (Toolset 17.x) | — |
| CMake | ≥ 3.25 | — |
| vcpkg | gebootstrappt | `F:\vcpkg` |

Weichen die Pfade ab, nicht `CMakePresets.json` ändern — stattdessen eine
`CMakeUserPresets.json` daneben legen, die das Preset `msvc2022` erbt und die
Pfade überschreibt. Diese Datei ist von der Versionsverwaltung ausgenommen.

## Bauen

```powershell
cmake --preset msvc2022
cmake --build --preset debug
```

Der erste Aufruf baut die vcpkg-Abhängigkeiten und dauert mehrere Minuten.

## Starten

```powershell
& "build\msvc2022\src\app\Debug\baludesk.exe"
```

## Tests

```powershell
ctest --preset debug
```
````

- [ ] **Step 14: Committen**

```powershell
git add .gitignore vcpkg.json CMakeLists.txt CMakePresets.json README.md src/app
git commit -m "build: Qt-Widgets-Geruest mit CMake-Presets und vcpkg-Manifest"
```

---

## Task 2: `core/` als Qt-freie Bibliothek mit `AppConfig`

**Files:**
- Create: `src/core/CMakeLists.txt`
- Create: `src/core/util/AppConfig.h`
- Create: `src/core/util/AppConfig.cpp`
- Create: `tests/CMakeLists.txt`
- Test: `tests/core/app_config_test.cpp`
- Modify: `CMakeLists.txt` (Zeile mit `add_subdirectory(src/app)` — `src/core` davor, `tests` danach ergänzen)

**Interfaces:**
- Consumes: aus Task 1 das Wurzel-`CMakeLists.txt` und die Presets
- Produces:
  - CMake-Ziel `baludesk_core` (STATIC, ohne Qt) und `baludesk_core_tests`
  - `struct baludesk::core::AppConfig` mit den Feldern `int pollIntervalSeconds`, `bool startMinimized`, `bool autostartWithWindows`, `bool notifyOnError`, `bool notifyOnSyncComplete`, `std::size_t logMaxFileBytes`, `std::size_t logMaxFiles`
  - `AppConfig AppConfig::clamped() const`
  - `bool operator==(const AppConfig&, const AppConfig&)`
  - Konstanten `AppConfig::kMinPollIntervalSeconds` (15), `kMaxPollIntervalSeconds` (3600), `kMinLogMaxFileBytes` (65536), `kMaxLogMaxFileBytes` (52428800), `kMaxLogFiles` (20)

---

- [ ] **Step 1: Den fehlschlagenden Test schreiben**

Datei `tests/core/app_config_test.cpp`:

```cpp
#include "core/util/AppConfig.h"

#include <gtest/gtest.h>

using baludesk::core::AppConfig;

TEST(AppConfigTest, DefaultsAreSane) {
    const AppConfig config;
    EXPECT_EQ(config.pollIntervalSeconds, 60);
    EXPECT_FALSE(config.startMinimized);
    EXPECT_FALSE(config.autostartWithWindows);
    EXPECT_TRUE(config.notifyOnError);
    EXPECT_FALSE(config.notifyOnSyncComplete);
    EXPECT_EQ(config.logMaxFiles, std::size_t{3});
}

TEST(AppConfigTest, ClampsPollIntervalBelowMinimum) {
    AppConfig config;
    config.pollIntervalSeconds = 1;
    EXPECT_EQ(config.clamped().pollIntervalSeconds, AppConfig::kMinPollIntervalSeconds);
}

TEST(AppConfigTest, ClampsPollIntervalAboveMaximum) {
    AppConfig config;
    config.pollIntervalSeconds = 999999;
    EXPECT_EQ(config.clamped().pollIntervalSeconds, AppConfig::kMaxPollIntervalSeconds);
}

TEST(AppConfigTest, LeavesValidPollIntervalAlone) {
    AppConfig config;
    config.pollIntervalSeconds = 120;
    EXPECT_EQ(config.clamped().pollIntervalSeconds, 120);
}

TEST(AppConfigTest, ClampsLogLimits) {
    AppConfig config;
    config.logMaxFileBytes = 1;
    config.logMaxFiles = 999;

    const AppConfig result = config.clamped();
    EXPECT_EQ(result.logMaxFileBytes, AppConfig::kMinLogMaxFileBytes);
    EXPECT_EQ(result.logMaxFiles, AppConfig::kMaxLogFiles);
}

TEST(AppConfigTest, ClampsLogFileCountToAtLeastOne) {
    AppConfig config;
    config.logMaxFiles = 0;
    EXPECT_EQ(config.clamped().logMaxFiles, std::size_t{1});
}

TEST(AppConfigTest, ClampingIsIdempotent) {
    AppConfig config;
    config.pollIntervalSeconds = 5;
    config.logMaxFiles = 999;

    const AppConfig once = config.clamped();
    const AppConfig twice = once.clamped();
    EXPECT_TRUE(once == twice);
}

TEST(AppConfigTest, ConstantsMatchTheSpecifiedLiterals) {
    // Verankert die Grenzwerte gegen die Spezifikation. Ohne diesen Test
    // prüfen alle übrigen nur gegen die Konstanten des Prüflings selbst -
    // eine falsch gesetzte Konstante bliebe dann unentdeckt.
    EXPECT_EQ(AppConfig::kMinPollIntervalSeconds, 15);
    EXPECT_EQ(AppConfig::kMaxPollIntervalSeconds, 3600);
    EXPECT_EQ(AppConfig::kMinLogMaxFileBytes, std::size_t{65536});
    EXPECT_EQ(AppConfig::kMaxLogMaxFileBytes, std::size_t{52428800});
    EXPECT_EQ(AppConfig::kMaxLogFiles, std::size_t{20});
}

TEST(AppConfigTest, LeavesLowerBoundaryValuesUntouched) {
    AppConfig config;
    config.pollIntervalSeconds = 15;
    config.logMaxFileBytes = std::size_t{65536};
    config.logMaxFiles = 1;

    const AppConfig result = config.clamped();
    EXPECT_EQ(result.pollIntervalSeconds, 15);
    EXPECT_EQ(result.logMaxFileBytes, std::size_t{65536});
    EXPECT_EQ(result.logMaxFiles, std::size_t{1});
}

TEST(AppConfigTest, LeavesUpperBoundaryValuesUntouched) {
    AppConfig config;
    config.pollIntervalSeconds = 3600;
    config.logMaxFileBytes = std::size_t{52428800};
    config.logMaxFiles = 20;

    const AppConfig result = config.clamped();
    EXPECT_EQ(result.pollIntervalSeconds, 3600);
    EXPECT_EQ(result.logMaxFileBytes, std::size_t{52428800});
    EXPECT_EQ(result.logMaxFiles, std::size_t{20});
}

TEST(AppConfigTest, ClampsJustOutsideTheBoundaries) {
    AppConfig config;

    config.pollIntervalSeconds = 14;
    EXPECT_EQ(config.clamped().pollIntervalSeconds, 15);

    config.pollIntervalSeconds = 3601;
    EXPECT_EQ(config.clamped().pollIntervalSeconds, 3600);
}

TEST(AppConfigTest, ClampsLogFileSizeAboveMaximum) {
    AppConfig config;
    config.logMaxFileBytes = std::size_t{500} * 1024 * 1024;
    EXPECT_EQ(config.clamped().logMaxFileBytes, std::size_t{52428800});
}

TEST(AppConfigTest, InequalityIsTheNegationOfEquality) {
    AppConfig left;
    AppConfig right;
    EXPECT_FALSE(left != right);

    right.pollIntervalSeconds = 120;
    EXPECT_TRUE(left != right);
}
```

Die letzten sechs Tests kamen aus dem Review der ersten Fassung. Sie schließen
drei Lücken, die eine reine Innen-und-Außen-Prüfung offenlässt: die Grenzwerte
selbst, die obere Schranke von `logMaxFileBytes`, und vor allem den
Zirkelschluss — ohne `ConstantsMatchTheSpecifiedLiterals` vergleichen alle
anderen Tests nur den Prüfling mit sich selbst.

- [ ] **Step 2: `src/core/CMakeLists.txt` anlegen**

```cmake
# WICHTIG: Dieses Ziel darf niemals Qt linken.
# Begründung in docs/superpowers/specs/2026-07-26-baludesk-qt-design.md, Abschnitt 2.4.
# Kein find_package(Qt6) und kein Qt6::-Ziel in dieser Datei.

add_library(baludesk_core STATIC
    util/AppConfig.cpp
)

target_include_directories(baludesk_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
```

- [ ] **Step 3: `tests/CMakeLists.txt` anlegen**

Zwei getrennte Binaries. `baludesk_core_tests` linkt kein Qt — läuft damit ohne jede Qt-DLL und macht die Qt-Freiheit von `core/` im Build sichtbar. Das App-Testbinary kommt in Task 4 dazu.

```cmake
find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(baludesk_core_tests
    core/app_config_test.cpp
)

target_link_libraries(baludesk_core_tests PRIVATE
    baludesk_core
    GTest::gtest
    GTest::gtest_main
)

gtest_discover_tests(baludesk_core_tests)
```

- [ ] **Step 4: Wurzel-`CMakeLists.txt` erweitern**

Die Zeile `add_subdirectory(src/app)` ersetzen durch:

```cmake
add_subdirectory(src/core)
add_subdirectory(src/app)
add_subdirectory(tests)
```

- [ ] **Step 5: Test laufen lassen und Fehlschlag bestätigen**

```powershell
cmake --preset msvc2022
```

Erwartet: FEHLER bereits beim Konfigurieren —
`Cannot find source file: util/AppConfig.cpp`.

Genau so soll es sein. Der Fehlschlag kommt aus CMake und nicht vom Übersetzer,
weil `add_library` eine Quelldatei nennt, die noch nicht existiert — das ist das
gleiche Signal: die Implementierung fehlt.

- [ ] **Step 6: `src/core/util/AppConfig.h` schreiben**

```cpp
#pragma once

#include <cstddef>

namespace baludesk::core {

// Einstellungswerte der Anwendung als schlichte Datenstruktur.
//
// Bewusst ohne Qt: der Kern kennt Qt nicht. Die Persistenz übernimmt
// baludesk::app::SettingsStore, der diese Struktur liest und schreibt.
struct AppConfig {
    // Abstand zwischen zwei Abfragen entfernter Änderungen.
    int pollIntervalSeconds = 60;

    // Beim Start direkt in den Infobereich minimieren.
    bool startMinimized = false;

    // Mit Windows automatisch starten.
    bool autostartWithWindows = false;

    // Benachrichtigung bei Fehlern und Konflikten.
    bool notifyOnError = true;

    // Benachrichtigung bei abgeschlossenen Sync-Läufen. Standardmäßig aus,
    // weil es sonst schnell lästig wird.
    bool notifyOnSyncComplete = false;

    // Obergrenze je Protokolldatei in Bytes.
    std::size_t logMaxFileBytes = std::size_t{5} * 1024 * 1024;

    // Anzahl aufbewahrter älterer Protokolldateien.
    std::size_t logMaxFiles = 3;

    static constexpr int kMinPollIntervalSeconds = 15;
    static constexpr int kMaxPollIntervalSeconds = 3600;
    static constexpr std::size_t kMinLogMaxFileBytes = std::size_t{64} * 1024;
    static constexpr std::size_t kMaxLogMaxFileBytes = std::size_t{50} * 1024 * 1024;
    static constexpr std::size_t kMaxLogFiles = 20;

    // Alle Werte auf ihre gültigen Bereiche zurechtstutzen.
    // Idempotent: clamped().clamped() == clamped().
    [[nodiscard]] AppConfig clamped() const;
};

bool operator==(const AppConfig& lhs, const AppConfig& rhs);
bool operator!=(const AppConfig& lhs, const AppConfig& rhs);

}  // namespace baludesk::core
```

- [ ] **Step 7: `src/core/util/AppConfig.cpp` schreiben**

```cpp
#include "core/util/AppConfig.h"

#include <algorithm>

namespace baludesk::core {

AppConfig AppConfig::clamped() const {
    AppConfig result = *this;

    result.pollIntervalSeconds =
        std::clamp(result.pollIntervalSeconds, kMinPollIntervalSeconds, kMaxPollIntervalSeconds);

    result.logMaxFileBytes =
        std::clamp(result.logMaxFileBytes, kMinLogMaxFileBytes, kMaxLogMaxFileBytes);

    result.logMaxFiles =
        std::clamp(result.logMaxFiles, std::size_t{1}, kMaxLogFiles);

    return result;
}

bool operator==(const AppConfig& lhs, const AppConfig& rhs) {
    return lhs.pollIntervalSeconds == rhs.pollIntervalSeconds
        && lhs.startMinimized == rhs.startMinimized
        && lhs.autostartWithWindows == rhs.autostartWithWindows
        && lhs.notifyOnError == rhs.notifyOnError
        && lhs.notifyOnSyncComplete == rhs.notifyOnSyncComplete
        && lhs.logMaxFileBytes == rhs.logMaxFileBytes
        && lhs.logMaxFiles == rhs.logMaxFiles;
}

bool operator!=(const AppConfig& lhs, const AppConfig& rhs) {
    return !(lhs == rhs);
}

}  // namespace baludesk::core
```

- [ ] **Step 8: Tests laufen lassen und grün bestätigen**

```powershell
cmake --build --preset debug
ctest --preset debug
```

Erwartet: 13 Tests, alle bestanden.

- [ ] **Step 9: Die Qt-Freiheit von `core/` im Build absichern**

Ans Ende von `src/core/CMakeLists.txt` anfügen:

```cmake
# Schutz gegen versehentliches Verlinken von Qt. Bricht die Konfiguration ab,
# sobald jemand ein Qt6::-Ziel an baludesk_core hängt.
#
# Zwei Grenzen dieser Prüfung, beide bewusst in Kauf genommen:
# 1. Sie erfasst nur direkt angehängte Ziele, kein Qt das über eine
#    Abhängigkeit hereinkäme.
# 2. Sie läuft an dieser Stelle, also während src/core verarbeitet wird.
#    Ein target_link_libraries(baludesk_core ... Qt6::...) aus einem später
#    verarbeiteten Unterverzeichnis — src/app kommt nach src/core — liefe
#    danach und bliebe unentdeckt.
# Zusammen mit der Regel "kein find_package(Qt6) in dieser Datei" reicht das
# für Stufe 0.
get_target_property(_core_link_libraries baludesk_core LINK_LIBRARIES)
if(_core_link_libraries MATCHES "Qt6::")
    message(FATAL_ERROR
        "baludesk_core darf kein Qt linken (Spec Abschnitt 2.4). "
        "Gefunden: ${_core_link_libraries}")
endif()
```

Neu konfigurieren und prüfen, dass die Absicherung nicht auslöst:

```powershell
cmake --preset msvc2022
```

Erwartet: `-- Configuring done`, keine Fehlermeldung.

*Warum keine Laufzeitprüfung:* Ein Nachweis am fertigen Binary wäre
aussagekräftiger als die Konfigurationsprüfung, ist hier aber nicht
praktikabel — vcpkg kopiert im Triplet `x64-windows` die DLLs von gtest,
spdlog und fmt neben das Testbinary, ein Start aus einem leeren Verzeichnis
scheiterte also an diesen und nicht an Qt. Die beiden Lücken der
Konfigurationsprüfung stehen im Kommentar über dem Code.

- [ ] **Step 10: Committen**

```powershell
git add src/core tests CMakeLists.txt
git commit -m "feat(core): AppConfig mit Bereichspruefung und Qt-freie Testsuite"
```

---

## Task 3: Logger mit Rotation und Obergrenze

**Files:**
- Create: `src/core/util/Logger.h`
- Create: `src/core/util/Logger.cpp`
- Modify: `src/core/CMakeLists.txt` (Quelldatei und spdlog ergänzen)
- Modify: `tests/CMakeLists.txt` (Testdatei ergänzen)
- Test: `tests/core/logger_test.cpp`

**Interfaces:**
- Consumes: `baludesk::core::AppConfig` aus Task 2 (die Felder `logMaxFileBytes` und `logMaxFiles` liefern die Parameter)
- Produces: Klasse `baludesk::core::Logger` mit den statischen Methoden
  `void init(const std::string& logDirectory, std::size_t maxFileBytes, std::size_t maxFiles)`,
  `void shutdown()`, `bool isInitialized()`, `std::string logFilePath()`, `void flush()`
  sowie den Vorlagen `info`, `warn`, `error`, `debug` mit spdlog-Formatzeichenketten.
  Die Protokolldatei heißt immer `baludesk.log`.

---

- [ ] **Step 1: Den fehlschlagenden Test schreiben**

Datei `tests/core/logger_test.cpp`:

```cpp
#include "core/util/Logger.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using baludesk::core::Logger;

namespace {

fs::path makeEmptyTempDir(const std::string& name) {
    const fs::path directory = fs::temp_directory_path() / ("baludesk_logger_test_" + name);
    fs::remove_all(directory);
    fs::create_directories(directory);
    return directory;
}

std::size_t countRegularFiles(const fs::path& directory) {
    std::size_t count = 0;
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            ++count;
        }
    }
    return count;
}

}  // namespace

class LoggerTest : public ::testing::Test {
protected:
    void TearDown() override { Logger::shutdown(); }
};

TEST_F(LoggerTest, CreatesLogFileOnInit) {
    const fs::path directory = makeEmptyTempDir("create");

    Logger::init(directory.string(), 1024 * 1024, 3);
    Logger::info("Testeintrag {}", 42);
    Logger::flush();

    EXPECT_TRUE(Logger::isInitialized());
    EXPECT_TRUE(fs::exists(directory / "baludesk.log"));
}

TEST_F(LoggerTest, ReportsLogFilePath) {
    const fs::path directory = makeEmptyTempDir("path");

    Logger::init(directory.string(), 1024 * 1024, 3);

    EXPECT_EQ(Logger::logFilePath(), directory.string() + "/baludesk.log");
}

TEST_F(LoggerTest, RotatesAndKeepsFileCountBounded) {
    const fs::path directory = makeEmptyTempDir("rotate");
    constexpr std::size_t kMaxBytes = 1024;
    constexpr std::size_t kMaxFiles = 3;

    Logger::init(directory.string(), kMaxBytes, kMaxFiles);

    const std::string filler(200, 'x');
    for (int i = 0; i < 200; ++i) {
        Logger::info("{} {}", i, filler);
    }
    Logger::flush();

    const std::size_t fileCount = countRegularFiles(directory);

    // Untere Schranke: es muss überhaupt rotiert worden sein.
    EXPECT_GE(fileCount, std::size_t{2}) << "Es wurde nicht rotiert";

    // Obere Schranke bewusst als kMaxFiles + 1 formuliert: spdlog hält die
    // aktive Datei plus maxFiles rotierte Stände. Der Test prüft, dass die
    // Zahl gedeckelt ist, nicht die genaue Zählweise von spdlog.
    EXPECT_LE(fileCount, kMaxFiles + 1) << "Mehr Dateien als die Obergrenze erlaubt";
}

TEST_F(LoggerTest, SecondInitDoesNotReplaceFirst) {
    const fs::path first = makeEmptyTempDir("first");
    const fs::path second = makeEmptyTempDir("second");

    Logger::init(first.string(), 1024 * 1024, 3);
    Logger::init(second.string(), 1024 * 1024, 3);

    EXPECT_EQ(Logger::logFilePath(), first.string() + "/baludesk.log");
}

TEST_F(LoggerTest, LoggingBeforeInitDoesNotCrash) {
    Logger::shutdown();
    EXPECT_FALSE(Logger::isInitialized());

    Logger::info("Eintrag ohne Initialisierung {}", 1);
    Logger::error("Fehler ohne Initialisierung");

    SUCCEED();
}
```

- [ ] **Step 2: `src/core/CMakeLists.txt` erweitern**

Vollständiger neuer Inhalt:

```cmake
# WICHTIG: Dieses Ziel darf niemals Qt linken.
# Begründung in docs/superpowers/specs/2026-07-26-baludesk-qt-design.md, Abschnitt 2.4.
# Kein find_package(Qt6) und kein Qt6::-Ziel in dieser Datei.

find_package(spdlog CONFIG REQUIRED)

add_library(baludesk_core STATIC
    util/AppConfig.cpp
    util/Logger.cpp
)

target_include_directories(baludesk_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(baludesk_core PUBLIC spdlog::spdlog)
```

- [ ] **Step 3: `tests/CMakeLists.txt` erweitern**

In der Quelldateiliste von `baludesk_core_tests` ergänzen:

```cmake
add_executable(baludesk_core_tests
    core/app_config_test.cpp
    core/logger_test.cpp
)
```

- [ ] **Step 4: Test laufen lassen und Fehlschlag bestätigen**

```powershell
cmake --preset msvc2022
```

Erwartet: FEHLER beim Konfigurieren — `Cannot find source file: util/Logger.cpp`.

- [ ] **Step 5: `src/core/util/Logger.h` schreiben**

```cpp
#pragma once

#include <spdlog/spdlog.h>

#include <cstddef>
#include <string>
#include <utility>

namespace baludesk::core {

// Protokollierung in eine rotierende Datei mit fester Obergrenze.
//
// init() muss einmal beim Programmstart aufgerufen werden, bevor weitere
// Threads laufen. Weitere Aufrufe von init() sind wirkungslos - der erste
// gewinnt. Aufrufe vor init() sind erlaubt und verpuffen still, damit ein
// vergessener Aufruf nicht zum Absturz führt.
class Logger {
public:
    static void init(const std::string& logDirectory,
                     std::size_t maxFileBytes,
                     std::size_t maxFiles);

    static void shutdown();

    [[nodiscard]] static bool isInitialized();

    // Vollständiger Pfad der aktiven Protokolldatei, leer wenn nicht
    // initialisiert. Die Trennzeichen sind immer Schrägstriche.
    [[nodiscard]] static std::string logFilePath();

    static void flush();

    template <typename... Args>
    static void debug(spdlog::format_string_t<Args...> format, Args&&... args) {
        if (auto* logger = raw()) {
            logger->debug(format, std::forward<Args>(args)...);
        }
    }

    template <typename... Args>
    static void info(spdlog::format_string_t<Args...> format, Args&&... args) {
        if (auto* logger = raw()) {
            logger->info(format, std::forward<Args>(args)...);
        }
    }

    template <typename... Args>
    static void warn(spdlog::format_string_t<Args...> format, Args&&... args) {
        if (auto* logger = raw()) {
            logger->warn(format, std::forward<Args>(args)...);
        }
    }

    template <typename... Args>
    static void error(spdlog::format_string_t<Args...> format, Args&&... args) {
        if (auto* logger = raw()) {
            logger->error(format, std::forward<Args>(args)...);
        }
    }

private:
    static spdlog::logger* raw();
};

}  // namespace baludesk::core
```

- [ ] **Step 6: `src/core/util/Logger.cpp` schreiben**

```cpp
#include "core/util/Logger.h"

#include <spdlog/sinks/rotating_file_sink.h>

#include <memory>
#include <mutex>

namespace baludesk::core {
namespace {

constexpr const char* kLoggerName = "baludesk";
constexpr const char* kLogFileName = "baludesk.log";

std::shared_ptr<spdlog::logger> g_logger;
std::string g_logFilePath;
std::mutex g_mutex;

}  // namespace

void Logger::init(const std::string& logDirectory,
                  std::size_t maxFileBytes,
                  std::size_t maxFiles) {
    std::lock_guard<std::mutex> lock(g_mutex);

    // Der erste Aufruf gewinnt. Ein zweiter würde sonst mitten im Betrieb
    // die Senke unter laufenden Threads austauschen.
    if (g_logger) {
        return;
    }

    g_logFilePath = logDirectory + "/" + kLogFileName;

    auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        g_logFilePath, maxFileBytes, maxFiles);

    g_logger = std::make_shared<spdlog::logger>(kLoggerName, std::move(sink));
    g_logger->set_level(spdlog::level::debug);
    g_logger->flush_on(spdlog::level::warn);
    g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_logger) {
        g_logger->flush();
        g_logger.reset();
    }
    g_logFilePath.clear();
}

bool Logger::isInitialized() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return static_cast<bool>(g_logger);
}

std::string Logger::logFilePath() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_logFilePath;
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_logger) {
        g_logger->flush();
    }
}

spdlog::logger* Logger::raw() {
    // Bewusst ohne Sperre: init() läuft beim Programmstart, bevor weitere
    // Threads existieren, und danach ändert sich der Zeiger nicht mehr.
    return g_logger.get();
}

}  // namespace baludesk::core
```

- [ ] **Step 7: Tests laufen lassen und grün bestätigen**

```powershell
cmake --build --preset debug
ctest --preset debug
```

Erwartet: 18 Tests, alle bestanden.

- [ ] **Step 8: Committen**

```powershell
git add src/core tests
git commit -m "feat(core): Logger mit Rotation und gedeckelter Dateianzahl"
```

---

## Task 4: Einstellungsspeicher und Verdrahtung

**Files:**
- Create: `src/app/SettingsStore.h`
- Create: `src/app/SettingsStore.cpp`
- Modify: `src/app/CMakeLists.txt` (Quelldatei ergänzen, `baludesk_core` linken)
- Modify: `src/app/main.cpp` (vollständig ersetzen)
- Modify: `tests/CMakeLists.txt` (zweites Testbinary ergänzen)
- Test: `tests/app/settings_store_test.cpp`

**Interfaces:**
- Consumes: `baludesk::core::AppConfig` (Task 2), `baludesk::core::Logger` (Task 3), `baludesk::app::MainWindow` (Task 1)
- Produces: Klasse `baludesk::app::SettingsStore` mit
  `SettingsStore()` (Standardort über `QSettings`, verlangt gesetzte Organisations- und Anwendungsnamen),
  `explicit SettingsStore(const QString& iniFilePath)` (für Tests),
  `core::AppConfig load() const` (liefert immer zurechtgestutzte Werte),
  `void save(const core::AppConfig& config)`;
  CMake-Ziel `baludesk_app_tests`

---

- [ ] **Step 1: Den fehlschlagenden Test schreiben**

Datei `tests/app/settings_store_test.cpp`:

```cpp
#include "app/SettingsStore.h"

#include <gtest/gtest.h>

#include <QSettings>
#include <QString>
#include <QTemporaryDir>

using baludesk::app::SettingsStore;
using baludesk::core::AppConfig;

TEST(SettingsStoreTest, ReturnsDefaultsWhenFileIsEmpty) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    const SettingsStore store(directory.filePath(QStringLiteral("settings.ini")));

    EXPECT_TRUE(store.load() == AppConfig{});
}

TEST(SettingsStoreTest, RoundTripsAllValues) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.ini"));

    AppConfig written;
    written.pollIntervalSeconds = 120;
    written.startMinimized = true;
    written.autostartWithWindows = true;
    written.notifyOnError = false;
    written.notifyOnSyncComplete = true;
    written.logMaxFileBytes = std::size_t{2} * 1024 * 1024;
    written.logMaxFiles = 5;

    {
        SettingsStore store(path);
        store.save(written);
    }

    const SettingsStore reopened(path);
    EXPECT_TRUE(reopened.load() == written);
}

TEST(SettingsStoreTest, ClampsOutOfRangeValuesFromFile) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.ini"));

    {
        QSettings raw(path, QSettings::IniFormat);
        raw.setValue(QStringLiteral("sync/pollIntervalSeconds"), 1);
        raw.setValue(QStringLiteral("logging/maxFiles"), 999);
        raw.sync();
    }

    const SettingsStore store(path);
    const AppConfig loaded = store.load();

    EXPECT_EQ(loaded.pollIntervalSeconds, AppConfig::kMinPollIntervalSeconds);
    EXPECT_EQ(loaded.logMaxFiles, AppConfig::kMaxLogFiles);
}

TEST(SettingsStoreTest, SaveWritesClampedValues) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.ini"));

    AppConfig unreasonable;
    unreasonable.pollIntervalSeconds = 999999;

    {
        SettingsStore store(path);
        store.save(unreasonable);
    }

    const QSettings raw(path, QSettings::IniFormat);
    EXPECT_EQ(raw.value(QStringLiteral("sync/pollIntervalSeconds")).toInt(),
              AppConfig::kMaxPollIntervalSeconds);
}
```

- [ ] **Step 2: `tests/CMakeLists.txt` erweitern**

Vollständiger neuer Inhalt. `baludesk_app_tests` braucht die Qt-DLLs zur Laufzeit; der Pfad wird aus dem Qt-Ziel abgeleitet und der Testumgebung mitgegeben, statt ihn fest einzutragen.

```cmake
find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

# --- Kern-Tests: linken bewusst kein Qt ---------------------------------
add_executable(baludesk_core_tests
    core/app_config_test.cpp
    core/logger_test.cpp
)

target_link_libraries(baludesk_core_tests PRIVATE
    baludesk_core
    GTest::gtest
    GTest::gtest_main
)

gtest_discover_tests(baludesk_core_tests)

# --- App-Tests: brauchen Qt --------------------------------------------
add_executable(baludesk_app_tests
    app/settings_store_test.cpp
)

target_link_libraries(baludesk_app_tests PRIVATE
    baludesk_app_lib
    GTest::gtest
    GTest::gtest_main
)

get_target_property(_qt_core_location Qt6::Core IMPORTED_LOCATION_RELEASE)
if(NOT _qt_core_location)
    get_target_property(_qt_core_location Qt6::Core IMPORTED_LOCATION)
endif()
get_filename_component(_qt_bin_dir "${_qt_core_location}" DIRECTORY)

# ENVIRONMENT_MODIFICATION statt ENVIRONMENT: der PATH enthält Semikola, und
# die würden in einer ENVIRONMENT-Liste als Trennzeichen zwischen Einträgen
# gelesen. path_list_prepend hängt den Qt-Pfad sauber vorne an, ohne den
# vorhandenen PATH anzufassen.
gtest_discover_tests(baludesk_app_tests
    DISCOVERY_MODE PRE_TEST
    PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_bin_dir}"
)
```

- [ ] **Step 3: `src/app/CMakeLists.txt` erweitern**

Den `add_library`-Block und die Verknüpfung ersetzen durch:

```cmake
add_library(baludesk_app_lib STATIC
    MainWindow.cpp
    SettingsStore.cpp
)

target_include_directories(baludesk_app_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(baludesk_app_lib PUBLIC Qt6::Widgets baludesk_core)
```

Der Rest der Datei bleibt unverändert.

- [ ] **Step 4: Test laufen lassen und Fehlschlag bestätigen**

```powershell
cmake --preset msvc2022
```

Erwartet: FEHLER beim Konfigurieren — `Cannot find source file: SettingsStore.cpp`.

- [ ] **Step 5: `src/app/SettingsStore.h` schreiben**

```cpp
#pragma once

#include <QSettings>
#include <QString>

#include "core/util/AppConfig.h"

namespace baludesk::app {

// Persistenz der Einstellungen. Übersetzt zwischen QSettings und der
// Qt-freien Struktur core::AppConfig.
//
// load() liefert immer zurechtgestutzte Werte, save() schreibt immer
// zurechtgestutzte Werte - eine von Hand verfremdete Datei kann die
// Anwendung damit nicht in einen unsinnigen Zustand bringen.
class SettingsStore {
public:
    // Standardort von QSettings. Verlangt, dass Organisations- und
    // Anwendungsname vorher über QCoreApplication gesetzt wurden.
    SettingsStore();

    // Feste INI-Datei. Wird von den Tests benutzt.
    explicit SettingsStore(const QString& iniFilePath);

    [[nodiscard]] core::AppConfig load() const;

    void save(const core::AppConfig& config);

private:
    mutable QSettings settings_;
};

}  // namespace baludesk::app
```

- [ ] **Step 6: `src/app/SettingsStore.cpp` schreiben**

```cpp
#include "app/SettingsStore.h"

namespace baludesk::app {
namespace {

constexpr const char* kPollIntervalSeconds = "sync/pollIntervalSeconds";
constexpr const char* kStartMinimized = "ui/startMinimized";
constexpr const char* kAutostartWithWindows = "ui/autostartWithWindows";
constexpr const char* kNotifyOnError = "notifications/onError";
constexpr const char* kNotifyOnSyncComplete = "notifications/onSyncComplete";
constexpr const char* kLogMaxFileBytes = "logging/maxFileBytes";
constexpr const char* kLogMaxFiles = "logging/maxFiles";

}  // namespace

SettingsStore::SettingsStore() = default;

SettingsStore::SettingsStore(const QString& iniFilePath)
    : settings_(iniFilePath, QSettings::IniFormat) {}

core::AppConfig SettingsStore::load() const {
    const core::AppConfig defaults;
    core::AppConfig config;

    config.pollIntervalSeconds =
        settings_.value(kPollIntervalSeconds, defaults.pollIntervalSeconds).toInt();
    config.startMinimized =
        settings_.value(kStartMinimized, defaults.startMinimized).toBool();
    config.autostartWithWindows =
        settings_.value(kAutostartWithWindows, defaults.autostartWithWindows).toBool();
    config.notifyOnError =
        settings_.value(kNotifyOnError, defaults.notifyOnError).toBool();
    config.notifyOnSyncComplete =
        settings_.value(kNotifyOnSyncComplete, defaults.notifyOnSyncComplete).toBool();
    config.logMaxFileBytes = static_cast<std::size_t>(
        settings_.value(kLogMaxFileBytes,
                        static_cast<qulonglong>(defaults.logMaxFileBytes)).toULongLong());
    config.logMaxFiles = static_cast<std::size_t>(
        settings_.value(kLogMaxFiles,
                        static_cast<qulonglong>(defaults.logMaxFiles)).toULongLong());

    return config.clamped();
}

void SettingsStore::save(const core::AppConfig& config) {
    const core::AppConfig sane = config.clamped();

    settings_.setValue(kPollIntervalSeconds, sane.pollIntervalSeconds);
    settings_.setValue(kStartMinimized, sane.startMinimized);
    settings_.setValue(kAutostartWithWindows, sane.autostartWithWindows);
    settings_.setValue(kNotifyOnError, sane.notifyOnError);
    settings_.setValue(kNotifyOnSyncComplete, sane.notifyOnSyncComplete);
    settings_.setValue(kLogMaxFileBytes, static_cast<qulonglong>(sane.logMaxFileBytes));
    settings_.setValue(kLogMaxFiles, static_cast<qulonglong>(sane.logMaxFiles));
    settings_.sync();
}

}  // namespace baludesk::app
```

- [ ] **Step 7: Tests laufen lassen und grün bestätigen**

```powershell
cmake --build --preset debug
ctest --preset debug
```

Erwartet: 22 Tests, alle bestanden.

- [ ] **Step 8: `src/app/main.cpp` vollständig ersetzen**

Jetzt wird verdrahtet: Einstellungen laden, Protokoll mit den geladenen Grenzwerten starten, Fenster zeigen.

```cpp
#include <QApplication>
#include <QDir>
#include <QStandardPaths>

#include "app/MainWindow.h"
#include "app/SettingsStore.h"
#include "core/util/AppConfig.h"
#include "core/util/Logger.h"

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);

    // Muss vor dem ersten QSettings-Zugriff stehen, weil QSettings den
    // Speicherort daraus ableitet.
    QCoreApplication::setOrganizationName(QStringLiteral("BaluHost"));
    QCoreApplication::setApplicationName(QStringLiteral("BaluDesk"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    const baludesk::app::SettingsStore settingsStore;
    const baludesk::core::AppConfig config = settingsStore.load();

    const QString logDirectory =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/logs");
    QDir().mkpath(logDirectory);

    baludesk::core::Logger::init(logDirectory.toStdString(),
                                 config.logMaxFileBytes,
                                 config.logMaxFiles);
    baludesk::core::Logger::info("BaluDesk {} gestartet",
                                 QCoreApplication::applicationVersion().toStdString());
    baludesk::core::Logger::info("Abfrageintervall: {} s", config.pollIntervalSeconds);

    baludesk::app::MainWindow window;
    window.show();

    const int exitCode = application.exec();

    baludesk::core::Logger::info("BaluDesk beendet, Rückgabewert {}", exitCode);
    baludesk::core::Logger::shutdown();

    return exitCode;
}
```

- [ ] **Step 9: Bauen und den Rauchtest durchführen**

```powershell
cmake --build --preset debug
& "build\msvc2022\src\app\Debug\baludesk.exe"
```

Fenster schließen, dann prüfen, dass das Protokoll geschrieben wurde:

```powershell
Get-Content "$env:LOCALAPPDATA\BaluHost\BaluDesk\logs\baludesk.log"
```

Erwartet: drei Zeilen — Start mit Version, Abfrageintervall 60 s, Beenden mit Rückgabewert 0.

- [ ] **Step 10: Abnahme von Stufe 0 vollständig durchlaufen**

```powershell
cd "F:\Programme (x86)\BaluDesk QT"
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
cmake --preset msvc2022
cmake --build --preset debug
ctest --preset debug
```

Abnahmekriterien aus der Spec, Abschnitt 12 Stufe 0 — alle vier müssen zutreffen:

1. `cmake --preset msvc2022` läuft ohne Fehler durch, vcpkg löst alle fünf Abhängigkeiten auf
2. `cmake --build --preset debug` erzeugt `baludesk.exe` ohne Warnungen aus eigenem Code
3. Die .exe startet, zeigt das Fenster und schreibt beim Beenden ein vollständiges Protokoll
4. `ctest --preset debug` meldet 22 bestandene Tests, 0 fehlgeschlagen

- [ ] **Step 11: Committen**

```powershell
git add src tests
git commit -m "feat(app): Einstellungsspeicher und Verdrahtung von Protokoll und Fenster"
```

---

## Nach Stufe 0

Stufe 1 (Pairing) bekommt einen eigenen Plan, geschrieben wenn Stufe 0 abgenommen ist —
so fließen die Erfahrungen aus dem Bauen in die nächste Planung ein
(Spec Abschnitt 12).

Was Stufe 0 bewusst offen lässt und Stufe 1 als Erstes braucht:

- `HttpClient` und der Gerätecode-Flow gegen `/api/desktop-pairing/*`
- `CredentialStore` für das Token im Windows Credential Manager
- `SyncEngineBridge` — entsteht erst, wenn es Callbacks zu übersetzen gibt
- Das Datenbankschema — erst in Stufe 2 nötig
