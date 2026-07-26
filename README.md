<div align="center">

# BaluDesk QT

**Desktop-Synchronisationsclient für [BaluHost](https://github.com/Xveyn/BaluHost) NAS**

[![License](https://img.shields.io/badge/license-MIT-purple.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-17-00599C.svg)](https://isocpp.org/)
[![Qt](https://img.shields.io/badge/Qt-6.8-41CD52.svg)](https://www.qt.io/)
[![Platform](https://img.shields.io/badge/platform-Windows-0078D6.svg)](#voraussetzungen)
[![Status](https://img.shields.io/badge/status-in%20Entwicklung-orange.svg)](#stand)

Hält lokale Ordner mit dem NAS im Gleichstand und läuft dabei unauffällig im Infobereich mit.

[Stand](#stand) · [Architektur](#architektur) · [Bauen](#bauen) · [Dokumentation](#dokumentation)

</div>

---

## Stand

**In Entwicklung.** Der Sync selbst ist noch nicht gebaut — was steht, ist das Fundament darunter:

| Stufe | Inhalt | Zustand |
|---|---|---|
| 0 | Gerüst: CMake, Qt-Fenster, Qt-freie Kernbibliothek, Protokoll, Einstellungen | ✅ abgenommen |
| 1a | Gesicherter Transport: OpenSSL, Zertifikat-Pinning, Anmeldeinformationsspeicher, HTTP-Client | ✅ abgenommen |
| 1b | Gerätecode-Pairing, Sitzung über Neustarts hinweg | ✅ abgenommen |
| 1c | Oberfläche: Dialoge, Hauptfenster, Abmelden, Token-Erneuerung | offen |
| 2 | Ein Ordner synchronisiert bidirektional | offen |
| 3 | Alltagstauglich: Infobereich, Autostart, mehrere Ordner | offen |
| 4–7 | Konflikte, Dashboard, Datei-Explorer, Auslieferung | offen |

Jede abgenommene Stufe ist gegen einen laufenden BaluHost-Server geprüft, nicht nur gegen Unittests. Was jeweils tatsächlich nachgewiesen wurde, steht in Abschnitt 14 der [Spezifikation](docs/superpowers/specs/2026-07-26-baludesk-qt-design.md).

## Was dieser Client ist — und was nicht

Im BaluHost-Umfeld gibt es drei Oberflächen, die bewusst getrennt bleiben:

| Anwendung | Läuft auf | Zweck |
|---|---|---|
| Web-App | Browser | Allgemeine Bedienung des NAS |
| Tauri-App | dem Server selbst | Administration mit Zugriff auf sensible Bereiche |
| **BaluDesk QT** | Arbeitsplatzrechnern | **Dateisynchronisation** |

Serveradministration — RAID, Lüfter, Power-Profile, Plugins — gehört ausdrücklich **nicht** hierher.

## Architektur

Ein Prozess, drei Schichten:

```
app/      Qt Widgets — Fenster, Infobereich, Dialoge
   ↕      Signals / Slots
bridge/   übersetzt Callbacks in Qt-Signale, wechselt den Thread
   ↕      std::function-Callbacks
core/     Sync, HTTP, TLS, Ablage — reines C++17, kennt Qt nicht
```

**`core/` darf Qt nicht kennen.** Das ist keine Ästhetik: die Tests laufen dadurch ohne
`QApplication`, und die Trennung ist im Bausystem abgesichert — die Konfiguration bricht
ab, sobald ein Qt-Ziel an die Kernbibliothek gehängt wird.

Kein Hintergrunddienst und kein IPC: liegt die Anwendung im Infobereich, synchronisiert
sie; wird sie beendet, ruht der Sync.

## Voraussetzungen

| Werkzeug | Version |
|---|---|
| Qt | 6.8.0, Kit `msvc2022_64` |
| Visual Studio Build Tools | 2022 (Toolset 17.x) |
| CMake | ≥ 3.25 |
| vcpkg | gebootstrappt, Manifest-Modus |

`CMakePresets.json` erwartet Qt unter `F:/Qt/6.8.0/msvc2022_64` und vcpkg unter
`F:/vcpkg`. Weichen die Pfade bei dir ab, **nicht die Presets ändern** — stattdessen eine
`CMakeUserPresets.json` daneben legen, die das Preset `msvc2022` erbt und nur die Pfade
überschreibt. Die Datei ist von der Versionsverwaltung ausgenommen.

## Bauen

```powershell
cmake --preset msvc2022
cmake --build --preset debug
ctest --preset debug
```

Der erste Aufruf baut die vcpkg-Abhängigkeiten aus den Quellen — OpenSSL und curl dauern
einige Minuten. Danach kommt alles aus dem Binärcache.

## Starten

```powershell
& "build\msvc2022\src\app\Debug\baludesk.exe"
```

Solange die Oberfläche der Stufe 1c fehlt, lässt sich das Pairing über das mitgelieferte
Prüfprogramm ausprobieren:

```powershell
& "build\msvc2022\tools\probe\Debug\baludesk_probe.exe" pair "nas.example.com"
& "build\msvc2022\tools\probe\Debug\baludesk_probe.exe" status
& "build\msvc2022\tools\probe\Debug\baludesk_probe.exe" logout
```

Es fährt denselben Code, den später die Dialoge fahren.

## Sicherheit

Zwei Eigenschaften, die den Entwurf prägen und in der Spezifikation begründet sind:

**Vertrauen beim ersten Kontakt.** Normalfall ist die reguläre Zertifikatsprüfung. Schlägt
sie fehl — selbstsigniert, LAN-Name, abgelaufen —, zeigt der Client den Fingerabdruck und
lässt ihn einmalig beim Pairing bestätigen. Weicht er später ab, bricht die Verbindung
**hart** ab, ohne erneute Nachfrage. Einen globalen Schalter „unsichere Verbindungen
erlauben" gibt es nicht.

**Zugangsdaten gehören dem Betriebssystem.** Zugangs- und Erneuerungstoken liegen getrennt
im Windows Credential Manager, niemals in einer Konfigurationsdatei. Was auf der Platte
liegt, sind Serveradresse, Benutzername und der bestätigte Zertifikat-Pin — nichts davon
ist ein Geheimnis.

## Dokumentation

| Dokument | Inhalt |
|---|---|
| [Spezifikation](docs/superpowers/specs/2026-07-26-baludesk-qt-design.md) | Entwurf, jede Entscheidung mit Begründung und verworfenen Alternativen, Server-Schnittstelle, Abnahmen |
| [Übergabe an Stufe 1c](docs/superpowers/uebergabe-an-stufe-1c.md) | Offene Punkte, Fallen und was als Nächstes zu entscheiden ist |
| [Pläne](docs/superpowers/plans/) | Implementierungspläne je Stufe, mit Testcode und Abnahmekriterien |

Die Dokumentation ist auf Deutsch, ebenso Kommentare und Protokolltexte im Quelltext.
Bezeichner und Dateinamen sind englisch.

## Mitwirken

Siehe [CONTRIBUTING.md](CONTRIBUTING.md). Sicherheitslücken bitte **nicht** über öffentliche
Issues melden — der Weg steht in [SECURITY.md](SECURITY.md).

## Lizenz

MIT — siehe [LICENSE](LICENSE).

## Verwandte Projekte

- **[BaluHost](https://github.com/Xveyn/BaluHost)** — der Server, den dieser Client anspricht
- **[BaluApp](https://github.com/Xveyn/BaluApp)** — Android-Anwendung
