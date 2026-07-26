# Mitwirken an BaluDesk QT

Danke fürs Interesse. Dieses Dokument beschreibt, wie hier gearbeitet wird — die
Konventionen sind ungewöhnlich genug, dass ein Blick vorher Zeit spart.

## Wie dieses Projekt entsteht

Jede Stufe durchläuft dieselbe Kette: **Spezifikation → Plan → Umsetzung → Review →
Abnahme gegen einen echten Server**. Die Spezifikation hält jede Entscheidung mit
Begründung *und* den verworfenen Alternativen fest, damit sie nicht alle drei Monate neu
verhandelt wird. Die Pläne enthalten den Testcode, bevor der Produktivcode entsteht.

Wer eine Entscheidung ändern will, ändert zuerst die Spezifikation. Ein Pull Request, der
einer festgehaltenen Entscheidung widerspricht, ohne sie anzufassen, wird zurückgewiesen —
nicht weil die Entscheidung heilig wäre, sondern weil sonst niemand mehr weiß, was gilt.

## Bauen und testen

```powershell
cmake --preset msvc2022
cmake --build --preset debug
ctest --preset debug
```

Voraussetzungen und Pfade stehen im [README](README.md). Weichen deine Pfade ab, lege eine
`CMakeUserPresets.json` an, statt `CMakePresets.json` zu ändern.

## Konventionen

**Die Schichtgrenze ist die wichtigste Regel.** `baludesk_core` darf Qt nicht kennen —
kein `QString`, kein `QObject`, kein Qt-Header. Die Kernbibliothek muss ohne
`QApplication` testbar bleiben. Das Bausystem bricht ab, wenn ein Qt-Ziel daran gehängt
wird; verlass dich trotzdem nicht darauf, sondern denk beim Schreiben daran.

**Sprache.** Bezeichner und Dateinamen englisch, Kommentare und Protokolltexte deutsch.

**Umlaute.** Echte UTF-8-Zeichen (`ü`, `ö`, `ä`, `ß`), niemals Behelfsschreibung wie `ue`.
Quelldateien als UTF-8 **ohne BOM**. Einzige Ausnahme: Git-Commit-Nachrichten bleiben
ASCII, weil PowerShell-5.1-Here-Strings unter Windows Umlaute zuverlässig verstümmeln.

**Includes** immer vollständig ab `src/`, also `#include "core/api/HttpClient.h"` — nie
relativ mit `../`.

**Warnungen.** Der Build läuft mit `/W4 /permissive- /utf-8` und muss frei von Warnungen
aus eigenem Code bleiben. Eine Warnung stillzulegen ist zulässig, wenn sie eng begrenzt
ist und ein Kommentar erklärt, warum.

**C++17.** Nicht höher, solange keine Notwendigkeit dafür begründet ist.

## Tests

Neuer Code kommt mit Tests. Zwei Dinge sind hier wichtiger als Abdeckungszahlen:

**Ein Test muss den Fehler fangen können, gegen den er behauptet zu schützen.** Wenn du
eine Sicherung einbaust, zeig dass der Test rot wird, wenn du die Sicherung entfernst. In
diesem Projekt sind mehrfach Tests aufgefallen, die grün blieben, obwohl der geprüfte
Code kaputt war — meist, weil beide Seiten dieselbe fehlerhafte Funktion benutzten.

**Tests fassen keine echten Betriebsmittel an.** Für den Anmeldeinformationsspeicher gibt
es `FakeSecretStore`, für HTTP `FakeTransport`. Ein Testlauf darf keine Spuren im System
des Entwicklers hinterlassen.

Zeitverhalten wird nicht durch Warten geprüft. Der `PairingClient` bekommt die aktuelle
Zeit hereingereicht und gibt zurück, wann er wieder gefragt werden will — dadurch sind
Intervalle, Fristen und Ratenbegrenzung ohne eine einzige echte Sekunde prüfbar. Neue
zeitabhängige Bausteine bitte genauso schneiden.

## Commits

Präfixe wie `feat(core):`, `fix(app):`, `build:`, `docs:`, `test:`. Die Nachricht erklärt
**warum**, nicht was — das steht im Diff. Nachrichten bleiben ASCII (siehe oben).

## Pull Requests

- Ein Anliegen je Pull Request
- `ctest --preset debug` muss grün sein, und die Zahl im PR nennen
- Berührt die Änderung eine Entscheidung aus der Spezifikation, gehört die Anpassung dazu
- Fällt dir etwas auf, das nicht zum Anliegen gehört: **nicht heimlich mitfixen**, sondern
  im PR benennen oder ein Issue aufmachen

## Sicherheitslücken

**Nicht** über öffentliche Issues, Diskussionen oder Pull Requests. Der Weg steht in
[SECURITY.md](SECURITY.md).

## Verhalten

Es gilt der [Verhaltenskodex](CODE_OF_CONDUCT.md).
