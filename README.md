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
