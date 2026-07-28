# BaluDesk QT — Design-Spezifikation

**Datum:** 2026-07-26
**Status:** Entwurf, vom Auftraggeber freigegeben
**Vorgänger:** `F:\Programme (x86)\BaluDesk` (Electron + C++, letzter Commit 2026-04-06)
**Server:** BaluHost v1.38.0, FastAPI — `D:\Programme (x86)\Baluhost`

---

## 1. Zweck und Abgrenzung

BaluDesk QT ist der Desktop-Synchronisationsclient für BaluHost NAS, neu gebaut als
Qt-6.8-Widgets-Anwendung in C++. Er hält lokale Ordner mit dem NAS im Gleichstand und
läuft dabei unauffällig im Infobereich mit.

Im BaluHost-Umfeld existieren drei Oberflächen, die klar getrennt bleiben:

| Anwendung | Läuft auf | Zweck |
|---|---|---|
| **Web-App** (`client/`) | Browser | Allgemeine Bedienung des NAS |
| **Tauri-App** (`client/src-tauri`) | dem Server selbst, KDE Plasma | Administration mit Zugriff auf sensible Bereiche (RAID-Arrays) |
| **BaluDesk QT** | Arbeitsplatzrechnern | Dateisynchronisation |

Tauri-App und Web-App teilen sich lediglich das Frontend; die Tauri-App ist **kein**
Desktop-Client. Serveradministration gehört ausdrücklich nicht in BaluDesk QT.

---

## 2. Getroffene Entscheidungen

Jede Entscheidung mit Begründung, damit sie später nicht neu verhandelt wird.

### 2.1 Qt Widgets statt QML/Quick

Die Anwendung besteht aus Listen, Formularen und Fortschrittsanzeigen — dafür ist die
Model/View-Architektur von Qt Widgets gebaut. Der gesamte wiederverwendbare Bestand ist
C++, mit Widgets bleibt es eine Codebasis ohne Sprachgrenze. Der Fußabdruck ist der
kleinste, den Qt anbietet, was bei einem Programm zählt, das dauerhaft mitläuft.

Erwogen und verworfen: QML/Quick. Es böte eine modernere Optik und Hot Reload, verlangt
aber eine zweite Sprache, verschiebt Fehler von der Übersetzungs- in die Laufzeit und
benötigt für `QSystemTrayIcon` ohnehin das QtWidgets-Modul. Der Grund, der dafür spräche —
eine visuell repräsentative Oberfläche — entfällt, weil das die Web-App leistet.

**Bewusst aufgegeben:** eine wirklich moderne Optik. Sollte später ein einzelner Bereich
aufwendiger werden, lässt er sich über `QQuickWidget` nachrüsten, ohne die App umzubauen.

### 2.2 Portierung durch Zerlegen statt Übernehmen

Der C++-Kern des Alt-Clients wird übernommen, beim Übertragen aber entlang seiner
Verantwortlichkeiten aufgeteilt. Die drei Monolithen `sync_engine.cpp` (83 KB),
`database.cpp` (59 KB) und `http_client.cpp` (51 KB) gehen nicht unverändert mit.

Erwogen und verworfen:

- **Unverändert übernehmen** — schneller zum ersten Ergebnis, erbt aber genau die
  Vermischung, die den Alt-Client unwartbar gemacht hat.
- **Vollständig neu im Qt-Idiom** (`QNetworkAccessManager`, `QtSql`, `QFileSystemWatcher`) —
  scheitert an `QFileSystemWatcher`: er kann nicht rekursiv überwachen und hält je Datei ein
  Handle offen, ist für Sync-Ordner mit tausenden Dateien also ungeeignet. Die
  plattformspezifischen Watcher müssten trotzdem geschrieben werden — und wären damit die
  einzige nachweislich funktionierende Komponente, die man wegwirft.

### 2.3 Ein Prozess, kein Hintergrunddienst

Der Sync läuft in Worker-Threads derselben Anwendung. Liegt sie im Infobereich,
synchronisiert sie; wird sie beendet, ruht der Sync. Damit entfällt der gesamte
IPC-Layer des Alt-Clients (`ipc_server*.cpp`, rund 140 KB) ersatzlos.

### 2.4 `core/` darf Qt nicht kennen

Keine Ästhetik, sondern zwei konkrete Folgen: die Tests laufen mit GTest ohne
`QApplication`, und die Portierung bleibt überprüfbar — solange nichts in `core/` ein
`QString` anfasst, ist der Kern der, der bereits funktioniert hat.

### 2.5 Windows zuerst

Nur Windows wird gebaut, getestet und paketiert. Die Linux- und macOS-Implementierungen
des Watchers und des Credential-Stores kommen als Quelltext mit, hängen aber hinter
`if(WIN32)` und werden nicht übersetzt. Spätere Freischaltung ist Build- und Testarbeit,
kein Umbau.

### 2.6 Geräte-Pairing gegen genau einen Server

Beim ersten Start Server-URL eingeben, dann Pairing über den Gerätecode-Flow. Ein Passwort
wird nie eingetippt, das Token liegt im Windows Credential Manager. Wer den Server wechselt,
meldet sich ab und paart neu.

Verworfen: Passwort-Login mit 2FA-Behandlung, mehrere Server-Profile, mDNS-Discovery. Der
Alt-Client hatte alle vier Wege nebeneinander.

### 2.7 Konflikte werden nicht blockierend gelöst

Der Alt-Code sieht für die manuelle Strategie einen Callback vor, der eine Entscheidung
zurückgibt — ein Worker-Thread bliebe also stehen, bis jemand klickt. Bei einer Anwendung,
deren Fenster oft gar nicht offen ist, ist das ein Aufhänger.

Stattdessen: Der Reconciler erkennt den Konflikt, schreibt ihn mit Status `pending` in die
Datenbank, überspringt diese eine Datei und setzt den Lauf fort. Die Auflösung wird
vermerkt und beim nächsten Lauf ausgeführt. **Kein Thread wartet je auf einen Menschen.**

Preis: eine konfliktbehaftete Datei bleibt unsynchronisiert liegen, bis der Nutzer
entscheidet. Das ist beabsichtigt — stilles Überschreiben wäre schlimmer.

### 2.8 Sprache und Umlaute im Quelltext

Bezeichner und Dateinamen englisch, Kommentare und Protokolltexte deutsch.

Umlaute werden als **echte UTF-8-Zeichen** geschrieben (`ü`, `ö`, `ä`, `ß`), nicht in
Behelfsschreibung wie `ue` oder `ss`. Das gilt für sichtbare Oberflächentexte,
Protokolltexte, Kommentare und Dokumentation gleichermaßen. Quelldateien werden als
UTF-8 ohne BOM gespeichert; `/utf-8` gehört zu den Übersetzeroptionen, damit MSVC sie
richtig liest.

**Einzige Ausnahme: Git-Commit-Nachrichten bleiben ASCII.** Sie werden unter Windows über
PowerShell-5.1-Here-Strings an `git` übergeben, und deren Kodierung verstümmelt Umlaute
zuverlässig. Ein `Geruest` in der Commit-Historie ist verschmerzbar, ein `GerÃ¼st` nicht.

Festgelegt am 2026-07-26, nachdem der Platzhaltertext des ersten Fensters sichtbar
„Geruest steht" statt „Gerüst steht" anzeigte.

### 2.9 TLS: strenge Prüfung mit Vertrauen beim ersten Kontakt

Der Normalfall ist die reguläre Zertifikatsprüfung gegen den System-Zertifikatsspeicher.
Der Server ist laut Deploy-Verzeichnis auf nginx mit Let's Encrypt ausgelegt; wer so
betreibt, merkt von dieser Regelung nichts.

Schlägt die Prüfung fehl — selbstsigniertes Zertifikat, Zugriff über LAN-Namen oder
IP-Adresse, abgelaufenes Zertifikat —, bricht der Client nicht einfach ab. Er zeigt den
**Fingerabdruck des Zertifikats** an und lässt ihn **einmalig beim Pairing** bestätigen.
Der bestätigte Abdruck wird gespeichert und ab dann geprüft.

Weicht der Abdruck später ab, ist das ein **harter Fehler**: Verbindung abbrechen,
Benachrichtigung, keine erneute Nachfrage im laufenden Betrieb. Ein Zertifikatswechsel am
Server verlangt bewusstes Neupaaren. Genau diese Härte macht das Verfahren erst sicher —
eine Rückfrage „trotzdem fortfahren?" mitten im Betrieb würde jeden MITM durchwinken.

Der gespeicherte Abdruck gehört zum gepaarten Server, ist kein Geheimnis und liegt
deshalb bei den Einstellungen, nicht im Credential Manager. Beim Abmelden wird er mit
gelöscht.

**Einen globalen Schalter „unsichere Verbindungen erlauben" gibt es nicht.** Der Alt-Client
sah so etwas vor; das ist der Unterschied zwischen „ich vertraue diesem einen Server" und
„ich prüfe gar nicht mehr".

Verworfen: nur reguläre Prüfung (der Client verbindet sich im Heimnetz dann gar nicht) und
HTTP gleichberechtigt zulassen (Zugangstoken und Dateiinhalte gingen unverschlüsselt durchs
Netz). HTTP bleibt allein für `localhost` erlaubt, damit Entwicklung gegen eine lokale
Dev-Instanz möglich bleibt.

**Umgesetzt und belegt in Stufe 1a** (siehe Abschnitt 15): curl spricht OpenSSL mit
`CURLSSLOPT_NATIVE_CA`, angeheftet wird der öffentliche Schlüssel (`sha256//…` für
`CURLOPT_PINNEDPUBLICKEY`), und das Zertifikat der Gegenstelle wird über
`CURLINFO_CERTINFO` ausgelesen. Das Anheften des Schlüssels statt des Zertifikats ist
Absicht: eine Erneuerung mit gleichem Schlüssel bricht den Pin nicht.

### 2.10 HTTP-Aufrufe blockieren, ohne Abbruchmöglichkeit

Ein Aufruf blockiert bis zu 30 Sekunden und lässt sich nicht mittendrin abbrechen. Ein
Abbrechen-Knopf wirkt deshalb *zwischen* zwei Abfragen, nicht innerhalb einer.

Bewusst so entschieden: gegen ein NAS im eigenen Netz antwortet der Server in
Millisekunden, das Zeitlimit greift praktisch nie. Die Alternative wäre ein
Fortschritts-Rückruf an `Transport::send`, mit dem sich ein laufender Aufruf abbrechen
ließe — Aufwand für einen Fall, der im LAN selten eintritt.

**Der Preis ist, dass Nachrüsten teuer wird:** `Transport::send` ist virtuell, ein
zusätzlicher Parameter bricht die Signatur. Wer das später doch braucht, ändert die
Schnittstelle und alle Implementierungen. Entschieden am 2026-07-26 im Abschlussreview
der Stufe 1a.

### 2.11 Neues Repository, harter Schnitt

Eigenes Git-Repo unter `F:\Programme (x86)\BaluDesk QT`. Der Alt-Client bleibt als
Referenz liegen. Keine Übernahme von Bestandsdaten: Nutzer paaren neu und legen ihre
Sync-Ordner neu an.

---

## 3. Funktionsumfang

### 3.1 Synchronisation (Kern)

- Bidirektionales Spiegeln lokaler Ordner mit NAS-Verzeichnissen
- **Richtung je Ordner** wählbar: bidirektional, nur hochladen, nur herunterladen
  (im Alt-Code als `SyncDirection` bereits vorhanden)
- Änderungserkennung lokal über Dateisystem-Ereignisse, entfernt über Abfrage
- Erkennung über SHA-256-Prüfsummen, nicht über Zeitstempel allein
- Stückweiser Upload großer Dateien
- **Konfliktauflösung mit Dialog**: lokale Version, Server-Version oder beide behalten

### 3.2 Dashboard (abgespeckt)

- Statusleiste: Zustand (online / schlafend / offline), Uptime, Wake-on-LAN-Knopf
- Vier Kacheln: CPU-Auslastung, RAM, Datenträger des Servers, eigener Speicherverbrauch
  auf dem NAS
- Sync-Zusammenfassung: Zustand, Übertragungsraten, offene Vorgänge, Ordnerliste

Die Uptime steht ausschließlich in der Statusleiste, nicht zusätzlich als Kachel — im
Alt-Client wurde sie an beiden Stellen angezeigt.

Entfallen gegenüber dem Alt-Client: Power-Profile, Dienste-Panel, Netzwerk-Graph,
Quick Share, RAID-Status, Plugin-Anzeige.

### 3.3 Datei-Explorer

Vollwertig: blättern, herunterladen, hochladen, umbenennen, löschen, Ordner anlegen.

*Anmerkung:* Dies ist der größte Einzelposten der Oberfläche und der einzige Bestandteil,
der sich mit dem Leitsatz „Fokus auf Sync" reibt — inhaltlich kann die Web-App das
bereits. Bewusst aufgenommen, damit der Zugriff im LAN ohne Browser möglich ist, und
deshalb als letzte Ausbaustufe eingeplant, damit er den Sync nicht aufhält.

### 3.4 Betriebsverhalten

- **Infobereich statt Taskleiste**: Fenster schließen versteckt, beendet nicht. Symbolfarbe
  zeigt den Zustand (synchron, aktiv, Konflikt, Fehler, pausiert), Kontextmenü mit Öffnen,
  Pausieren, Ordner öffnen, Beenden
- **Autostart mit Windows**, minimiert, abschaltbar
- **Desktop-Benachrichtigungen** bei Konflikten und Fehlern; Erfolgsmeldungen abschaltbar
- **Kein Aktivitätsprotokoll in der Oberfläche** — stattdessen rotierende Protokolldatei
  mit Knopf „Protokollordner öffnen" in den Einstellungen

---

## 4. Nicht-Ziele

Ausdrücklich nicht Bestandteil, damit später nicht diskutiert wird, ob es vergessen wurde:

- **Serververwaltung** — RAID, Lüfter, Power-Profile, Dienste, Plugins, VPN, SSH.
  Einzige Ausnahme ist Wake-on-LAN, und das ist Voraussetzung für Sync, nicht Administration.
- Mehrere Server-Profile, mDNS-Discovery, Passwort-Login mit 2FA
- Selektiver Sync und Bandbreitenlimit — auf dem Server vorhanden, im Client weggelassen
- Aktivitätsprotokoll in der Oberfläche
- Automatische Updates — Aktualisierung über einen neuen Installer
- Versionsverlauf, LAN-Sync, Delta-Sync auf Blockebene
- Datenübernahme aus dem Alt-Client
- Automatisierte Oberflächentests

---

## 5. Architektur

Ein Prozess, drei Schichten:

```
┌─────────────────────────────────────────────┐
│  app/     Qt Widgets — Fenster, Tray,       │  ruft core/ für Werttypen
│           Dialoge, Models                    │  und Logger direkt auf
└───────────────────┬─────────────────────────┘
                    │  Signals / Slots
┌───────────────────┴─────────────────────────┐
│  bridge/  SyncEngineBridge : QObject        │  einzige Stelle, an der
│           Callbacks → Signale, Thread-Wechsel│  beide Welten sich berühren
└───────────────────┬─────────────────────────┘
                    │  std::function-Callbacks
┌───────────────────┴─────────────────────────┐
│  core/    Sync, HTTP, DB, Keychain          │  kennt Qt nicht
│           reines C++17, statische Bibliothek │
└─────────────────────────────────────────────┘
```

Die `bridge/`-Schicht existiert, um Rückrufe aus `core/`-Arbeiterthreads sicher in den
Qt-GUI-Thread zu holen und dort als Signale erscheinen zu lassen - nicht, um jeden
Zugriff von `app/` auf `core/` zu verschleiern. Reine Werttypen wie `AppConfig` und der
threadsichere, statische `Logger` brauchen keinen Thread-Wechsel und werden deshalb
direkt aus `app/` verwendet, ohne den Umweg über `bridge/`.

### 5.1 Verzeichnisstruktur

```
BaluDesk QT/
├── CMakeLists.txt
├── CMakePresets.json        # msvc2022-debug/-release, Qt- und vcpkg-Pfade
├── vcpkg.json               # curl, sqlite3, nlohmann-json, spdlog, gtest
├── src/
│   ├── core/
│   │   ├── sync/            # SyncCoordinator, Reconciler, TransferWorker,
│   │   │                    # FileWatcher (+ _windows/_linux/_macos), ConflictResolver
│   │   ├── api/             # HttpClient, CurlHandlePool, RateLimiter, RemoteFileService,
│   │   │                    # ServerUrl, TlsPolicy, TrustNegotiation
│   │   ├── auth/            # PairingClient, AuthenticatedClient, Session,
│   │   │                    # SessionRepository, SessionPaths
│   │   ├── db/              # SyncDatabase
│   │   └── util/            # Sha256, CredentialStore, Logger, TlsTrust, WakeOnLan
│   ├── bridge/              # AuthBridge, SyncEngineBridge, FileBrowserBridge
│   └── app/
│       ├── MainWindow, TrayController, NotificationCenter
│       ├── pairing/ dashboard/ sync/ explorer/ settings/ conflicts/
│       └── models/          # SyncFolderModel, ConflictModel, RemoteFileModel
├── tests/
│   ├── core/                # GTest gegen core/, mit support/ und data/
│   ├── bridge/              # GTest gegen bridge/
│   └── app/                 # GTest gegen app/
├── tools/probe/             # baludesk_probe — Ablauf von der Kommandozeile
├── resources/               # SVG-Symbole, QSS-Stylesheet
└── docs/superpowers/specs/  # dieses Dokument
```

Stand nach Stufe 1c gebaut: `core/api/`, `core/auth/`, `core/util/`, `bridge/`
(darin bisher nur `AuthBridge`), `app/pairing/`, `tools/probe/` und die drei
Testverzeichnisse. `core/sync/`, `core/db/` und die übrigen `app/`-Bereiche
kommen ab Stufe 2.

Die Testverzeichnisse spiegeln die Schichten. Ein Include in Anführungszeichen
sucht zuerst neben der einbindenden Datei: aus `tests/bridge/` heißt es deshalb
`"core/support/FakeTransport.h"`, nicht `"support/FakeTransport.h"`.

### 5.2 Build

CMake mit Presets. Qt 6.8.0 `msvc2022_64` aus der vorhandenen Installation unter `F:\Qt`
über `CMAKE_PREFIX_PATH`; alle übrigen Abhängigkeiten über **vcpkg im Manifest-Modus**,
also als `vcpkg.json` im Repo statt als Submodul im Baum.

Vorhandene Werkzeuge auf der Zielmaschine: Qt 6.8.0 (msvc2022_64, mingw_64, llvm-mingw,
Android, WASM), Qt Creator, Qt Design Studio, Ninja, CMake, Visual Studio Build Tools 2022,
vcpkg (Stand 2025-12-16). Es ist kein Setup-Aufwand nötig.

**Qt-Module:** Core, Gui, Widgets, Svg — dazu Network, das ausschließlich für `QUdpSocket`
beim Wake-on-LAN gebraucht wird. HTTP läuft über libcurl.

---

## 6. Komponenten

### 6.1 `core/`

| Einheit | Aufgabe | Hängt ab von | Herkunft |
|---|---|---|---|
| `SyncCoordinator` | Lebenszyklus, Ordnerregistrierung, Läufe auslösen, Gesamtzustand | DB, Watcher, Reconciler, Transfer | aus `sync_engine.cpp` |
| `Reconciler` | lokalen Stand, DB-Metadaten und Serverliste vergleichen → Arbeitsliste | DB, HttpClient | aus `sync_engine.cpp` + `change_detector.cpp` |
| `TransferWorker` | Arbeitsposten ausführen, Fortschritt melden, mit Backoff wiederholen | HttpClient, DB | aus `sync_engine.cpp` |
| `FileWatcher` | Verzeichnisse überwachen, Ereignisse entprellen (500 ms) | — | unverändert |
| `ConflictResolver` | Strategie anwenden | DB, HttpClient | unverändert |
| `SyncDatabase` | vier Tabellen (siehe 8.) | sqlite3 | aus `database.cpp`, eingedampft |
| `HttpClient` | REST, Token-Erneuerung, stückweiser Upload | libcurl | aus `http_client.cpp`, gekürzt |
| `RemoteFileService` | Verzeichnisse und Dateien für den Explorer | HttpClient | neu |
| `CredentialStore` | Windows Credential Manager | — | unverändert |
| `Sha256`, `Logger` | Prüfsummen, Protokoll | — | unverändert |
| `WakeOnLan` | Magic Packet als UDP-Broadcast | QUdpSocket | neu |

`Reconciler` zieht den größten Gewinn aus der Zerlegung: er bekommt lokalen Zustand,
DB-Zustand und Serverliste hinein und gibt eine Arbeitsliste heraus — ohne Netzwerk,
Dateisystem oder Datenbank testbar. Damit wird die Sync-Logik prüfbar, was sie in
`sync_engine.cpp` nie war.

### 6.2 Entfällt ersatzlos aus dem Alt-Client

`ipc_server.cpp`, `ipc_server_fixed.cpp`, `ipc_server_temp.cpp` und `.backup` (~140 KB) ·
`ssh_service`, `vpn_service` · `raid_info`, `system_info`, `mock_data_provider` ·
`file_watcher_v2` und die übrigen Dubletten · das gesamte Electron-Frontend (~280 KB TSX).

### 6.3 Bridge

`SyncEngineBridge : QObject` besitzt den `SyncCoordinator` und übersetzt dessen Callbacks
in Qt-Signale. `FileBrowserBridge` tut dasselbe für den Explorer.

**Signale:** `statusChanged(SyncStats)`, `fileChanged(FileEvent)`, `errorOccurred(QString)`,
`conflictDetected(ConflictInfo)`, `authRequired()`
**Slots:** `addFolder`, `removeFolder`, `pause`, `resume`, `triggerSync`, `resolveConflict`

---

## 7. Datenfluss und Threading

### 7.1 Threads

- **GUI-Thread** — ausschließlich Qt-Widgets, fasst nie `core/` direkt an
- **Koordinator-Thread** — die Sync-Schleife, wartet auf Watcher-Ereignisse oder Poll-Timer
- **Transfer-Threads** — begrenzter Pool für Übertragungen

Die Callbacks aus `core/` feuern aus Koordinator- und Transfer-Threads. `SyncEngineBridge`
wandelt sie in Qt-Signale, die per `Qt::QueuedConnection` im GUI-Thread ankommen; die
übergebenen Typen (`SyncStats`, `FileEvent`, `ConflictInfo`) werden dafür mit
`qRegisterMetaType` angemeldet.

**Dies ist die einzige Stelle im Programm, an der Thread-Grenzen überschritten werden** —
und damit die einzige, die bei Nebenläufigkeitsfehlern zu prüfen ist.

Der Explorer nutzt einen getrennten curl-Handle aus dem Pool, damit ein Verzeichniswechsel
nicht hinter einem laufenden Großupload wartet. Sync und Stöbern teilen sich das Token,
sonst nichts.

### 7.2 Abläufe

**Lokale Änderung:** Watcher meldet Ereignis → 500 ms Entprellung → Warteschlange →
Koordinator weckt auf → Reconciler für den betroffenen Ordner → Arbeitsliste → Transfer
lädt hoch → DB → Statusmeldung → Bridge → Oberfläche.

**Entfernte Änderung:** Poll-Timer (Vorgabe 60 s, in den Einstellungen änderbar) →
`POST /api/sync/changes` → Reconciler → Transfer lädt herunter → DB → Oberfläche.

**Neuer Ordner:** vollständiger Erstabgleich — lokal durchlaufen, Serverliste holen,
Prüfsummen bilden, Arbeitsliste erzeugen. Der teuerste Vorgang, deshalb mit
Fortschrittsanzeige und abbrechbar.

---

## 8. Datenmodell

SQLite, vier Tabellen. Aus dem Alt-Schema entfallen die Tabellen für RAID, Dienste,
VPN-Profile, Server-Profile und `activity_log`.

- **`sync_folders`** — `id`, `local_path`, `remote_path`, `direction`, `conflict_strategy`,
  `enabled`, `status`, `created_at`, `last_sync`
- **`file_metadata`** — `path`, `folder_id`, `hash`, `size`, `modified_at`, `is_directory`,
  `sync_status`
- **`sync_state`** — `folder_id`, `last_local_scan`, `last_remote_scan`, `pending_uploads`,
  `pending_downloads`
- **`conflicts`** — `id`, `folder_id`, `path`, `local_hash`, `remote_hash`,
  `local_modified`, `remote_modified`, `detected_at`, `state` (`pending` / `resolved`),
  `resolution`, `resolved_at`

Indizes auf `file_metadata(folder_id)`, `file_metadata(sync_status)` und `conflicts(state)`.

**Die Datenbank wird nach jeder Datei geschrieben, nicht am Ende eines Laufs.** Stürzt die
Anwendung mitten im Lauf ab, geht höchstens eine Datei verloren, nicht der Fortschritt.

---

## 9. Server-Schnittstelle

Gegen BaluHost v1.38.0 verifiziert (Stand 2026-07-26). Zwei Abweichungen gegenüber dem
Alt-Client, die dort veraltet waren:

- `/api/sync/changes` ist **POST**, nicht GET
- `/api/files/rename` und `/api/files/move` sind **PUT**, nicht POST

| Zweck | Endpunkt |
|---|---|
| Pairing anstoßen | `POST /api/desktop-pairing/device-code` |
| Pairing abfragen | `POST /api/desktop-pairing/poll` |
| Token erneuern | `POST /api/auth/refresh` |
| Angemeldeten Nutzer lesen | `GET /api/auth/me` |
| Gerät registrieren | `POST /api/sync/register-desktop` |
| Entfernte Änderungen | `POST /api/sync/changes` |
| Verzeichnis auflisten | `GET /api/files/list` |
| Herunterladen | `GET /api/files/download/{pfad}` |
| Hochladen | `POST /api/files/upload` |
| Ordner anlegen | `POST /api/files/folder` |
| Umbenennen | `PUT /api/files/rename` |
| Verschieben | `PUT /api/files/move` |
| Löschen | `DELETE /api/files/delete` |

Der Server bietet für zwei dieser Vorgänge mehrere gleichwertige Endpunkte an —
`/api/files/mkdir` neben `/folder`, sowie `POST /delete`, `DELETE /raw/{pfad}` und
`DELETE /{pfad}` neben `DELETE /delete`. Der Client verwendet **ausschließlich die oben
genannten**, damit nicht an verschiedenen Stellen unterschiedliche Wege entstehen.
| Einhängepunkte | `GET /api/files/mountpoints` |
| Eigener Speicherverbrauch | `GET /api/files/user/root-usage` |
| Freier Speicher | `GET /api/files/storage/available` |
| Systeminformationen | `GET /api/system/info` |
| Erreichbarkeitsprüfung | `GET /api/health` |

Ein OpenAPI-Schema ist im BaluHost-Repo nicht eingecheckt; FastAPI liefert es zur Laufzeit
unter `/openapi.json`. Bei Schema-Zweifeln ist das die maßgebliche Quelle.

### 9.1 Wake-on-LAN

Im Alt-Client war der WOL-Knopf **tot**: das Frontend schickte `power_action: 'wol'` an das
C++-Backend, dort existierte keine Implementierung. Auf dem Server läuft WOL über die
FritzBox (`fritzbox_wol.py`, TR-064) — dieser Endpunkt hängt aber am schlafenden Server und
ist genau dann nicht erreichbar, wenn er gebraucht wird.

BaluDesk QT sendet das Magic Packet daher **selbst**, als UDP-Broadcast auf Port 9. Die
MAC-Adresse des Servers wird gespeichert, solange er erreichbar ist.

**Voraussetzungen, die außerhalb der Anwendung liegen:** Client und Server im selben
Netzsegment, WOL im BIOS und im Netzwerktreiber des Servers aktiviert. Beim allerersten
Start vor dem ersten Serverkontakt ist WOL nicht verfügbar, weil die MAC noch fehlt.

---

## 10. Fehlerbehandlung

Bestimmender Umstand: **der Server schläft regelmäßig.** Nicht-Erreichbarkeit ist ein
normaler Betriebszustand, kein Fehler.

| Klasse | Verhalten | Sichtbarkeit |
|---|---|---|
| Server offline oder schlafend | Poll-Intervall wächst gestaffelt von 60 s auf max. 5 min; bei Antwort sofort zurück auf normal | Symbol grau, Statusleiste „Offline", WOL aktiv. Keine Benachrichtigung |
| Netzwerkfehler im Transfer | `retryWithBackoff`, drei Versuche (1 s / 2 s / 4 s), danach im nächsten Lauf erneut | erst nach mehreren gescheiterten Läufen |
| Token abgelaufen | `withTokenRefresh` erneuert still und wiederholt | unsichtbar |
| Erneuerung gescheitert, Pairing widerrufen | Sync **pausiert vollständig**, Signal `authRequired` → Pairing-Ansicht | Symbol rot, Benachrichtigung |
| Kontingent voll (507) | Uploads 10 min aussetzen, Downloads laufen weiter | Symbol gelb, Benachrichtigung mit Speicherstand |
| Ratenbegrenzung (429) | `Retry-After` auswerten und einhalten | unsichtbar |
| Datei lokal gesperrt | drei Versuche, dann überspringen und Lauf **fortsetzen** | nur Protokoll |
| Lokaler Datenträger voll | Downloads für den Ordner anhalten | Symbol rot, Benachrichtigung |
| Konflikt | kein Fehler, eigener Zustand (siehe 2.7) | Symbol gelb, Benachrichtigung, Konfliktliste |

Zwei durchgehende Prinzipien: **ein einzelner Fehler bricht nie einen ganzen Lauf ab**, und
**die Datenbank wird nach jeder Datei geschrieben**.

Protokollierung über spdlog mit Rotation und fester Obergrenze. Im Alt-Client wuchsen die
Protokolle unbegrenzt (1 MB aktiv plus drei Archive à 5 MB); das wird von vornherein
gedeckelt.

---

## 11. Teststrategie

### 11.1 Direkt übernehmbar

Weil die zugehörigen Klassen kaum angefasst werden: `conflict_resolver_test.cpp` (18 KB),
`database_test.cpp` (27 KB), `file_watcher_test.cpp` (8,5 KB),
`credential_store_test.cpp` (11 KB), `change_detector_hash_test.cpp`, `logger_test.cpp`.

### 11.2 Neu zu schreiben

Die fünf bestehenden `sync_engine_*`-Tests (rund 70 KB) dienen als Vorlage, nicht als
Übernahme, weil die Klasse in drei Teile zerfällt.

- **`Reconciler`-Tests — der Schwerpunkt.** Lokaler Zustand, DB-Zustand und Serverliste als
  schlichte Datenstrukturen hinein, Arbeitsliste heraus. Kein Netz, kein Dateisystem.
  Grenzfälle: beidseitig geändert, lokal gelöscht während entfernt geändert, Umbenennung,
  Groß-/Kleinschreibung, Zeitstempel-Abweichung zwischen Windows und Linux.
- **`TransferWorker`-Tests** gegen einen Fake-HttpClient: Wiederholverhalten,
  Fortschrittsmeldungen, Abbruch.
- **`SyncCoordinator`-Tests** mit Fakes für Watcher und Transfer: kein doppelter Lauf;
  ein während eines Laufs angeforderter Lauf wird korrekt nachgeholt.

### 11.3 Bewusste Lücken

- **`HttpClient` wird nicht gegen den echten Server automatisiert getestet.** Ein
  vollwertiger Fake-Server kostet mehr, als er hier einbringt. Getestet wird die
  Antwort-*Auswertung* (gespeicherte JSON-Antworten hinein, geparste Strukturen heraus);
  das Zusammenspiel mit BaluHost über eine dokumentierte manuelle Abnahmeliste gegen eine
  laufende Dev-Instanz.
- **Keine automatisierten Oberflächentests.** Bei einem Ein-Personen-Projekt ist der Ertrag
  gegenüber einer Klickliste gering. Entscheidend ist, dass die Logik geprüft ist — und die
  liegt vollständig in `core/`.

---

## 12. Ausbaustufen

Der Konflikt-Dialog kommt **vor** dem Dashboard: Konflikte gehören zum Sync, ohne
Auflösungsmöglichkeit bleiben betroffene Dateien dauerhaft liegen. Bleibt das Projekt
irgendwo stehen, soll es mit funktionierendem Sync stehenbleiben.

| Stufe | Inhalt | Fertig, wenn |
|---|---|---|
| **0 — Gerüst** | Repo, CMake mit Presets, vcpkg-Manifest, leeres Qt-Fenster, `core/` als statische Bibliothek, Logger, Einstellungsspeicher, GTest | `cmake --build` erzeugt eine startende .exe und eine grüne Testsuite |
| **1 — Pairing** | Server-URL, Gerätecode-Flow, Token im Credential Manager, „verbunden mit … als …", Abmelden | Neustart führt ohne erneutes Pairing zu einer authentifizierten Sitzung |
| **2 — Ein Ordner synct** | DB, Watcher, Reconciler, Transfer, bidirektional, Erstabgleich mit Fortschritt; grobe Oberfläche | Datei lokal anlegen erscheint auf dem NAS und umgekehrt; Neustart mitten im Lauf verliert keinen Fortschritt |
| **3 — Alltagstauglich** | Tray mit Zustandsfarbe und Menü, Autostart, Benachrichtigungen, mehrere Ordner, hinzufügen/entfernen/pausieren, Richtung je Ordner, Remote-Ordner-Picker | läuft eine Woche unbeaufsichtigt, ohne Aufmerksamkeit zu fordern |
| **4 — Konflikte** | Konfliktliste, Auflösungsdialog, Wiederaufnahme beim nächsten Lauf | gleichzeitige Änderung auf beiden Seiten führt zu keinem stillen Datenverlust |
| **5 — Dashboard** | Statusleiste mit Zustand, Uptime und WOL; CPU/RAM/Disk-Kacheln; eigener Speicherverbrauch; Sync-Zusammenfassung | zeigt bei schlafendem Server sinnvolle Zustände statt Fehlermeldungen |
| **6 — Explorer** | blättern, herunterladen, hochladen, umbenennen, löschen, Ordner anlegen | bedienbar ohne Rückgriff auf die Web-App |
| **7 — Auslieferung** | `windeployqt`, Installer, Deinstallation, Versionsanzeige | auf einem frischen Windows-Rechner ohne Qt installierbar und lauffähig |

**Ab Stufe 3 ist das Programm nutzbar.** Alles danach macht es besser, nicht erst brauchbar.

Der Umfang ist zu groß für einen einzigen Implementierungsplan. **Jede Stufe bekommt einen
eigenen Plan**, geschrieben jeweils dann, wenn die vorhergehende Stufe abgenommen ist — so
fließen die Erkenntnisse aus dem Bauen in die Planung der nächsten Stufe ein, statt vorab
acht Pläne zu schreiben, von denen sechs veralten.

---

## 13. Offene Punkte

1. **Code-Signierung.** Ohne Zertifikat warnt Windows SmartScreen bei jedem Installer.
   Erträglich im Eigengebrauch, aber zu benennen. Entscheidung offen.
2. **Wake-on-LAN in der Praxis.** Ob Client und Server im selben Netzsegment liegen und WOL
   im BIOS und Netzwerktreiber aktiv ist, muss der Betreiber prüfen (siehe 9.1).
3. **Automatische Updates** sind als Nicht-Ziel gesetzt. Nachträglicher Einbau ist teurer
   als von Anfang an — die Entscheidung wurde bewusst so getroffen.

---

## 14. Abgenommene Stufen

Was tatsächlich geprüft wurde — nicht was geplant war.

### Stufe 0 — Gerüst (abgenommen 2026-07-26)

25 Tests. Aus gelöschtem Build-Verzeichnis von Null konfiguriert, gebaut, gestartet und
getestet. Fenster erscheint, Protokoll wird geschrieben und ist byte-geprüft gültiges
UTF-8. Die Absicherung gegen Qt in `baludesk_core` wurde erst zum Auslösen gebracht
(probeweise `Qt6::Core` angehängt → `FATAL_ERROR`), bevor sie als wirksam galt.

### Stufe 1a — Transport und Zertifikatsvertrauen (abgenommen 2026-07-26)

60 Tests. Die Vertrauensmechanik aus Abschnitt 2.9 wurde gegen den echten Server und
gegen öffentliche Testdienste belegt, nicht nur in Unittests behauptet:

| Prüfung | Ergebnis |
|---|---|
| Gültiges Zertifikat (`nas.example.com`, Let's Encrypt) | verbunden, Status 200 |
| Name passt nicht (`nas.local`, dasselbe Zertifikat) | `CertificateUntrusted`, Fingerabdruck und Pin ausgelesen |
| Selbstsigniert (`self-signed.badssl.com`) | `CertificateUntrusted`, Fingerabdruck und Pin ausgelesen |
| Unbekannte Wurzel (`untrusted-root.badssl.com`) | `CertificateUntrusted`, Fingerabdruck und Pin ausgelesen |
| Mit dem ausgelesenen Pin verbinden | verbunden, Status 200 |
| Ein Zeichen des Pins verfälscht | `CertificatePinMismatch` — **eigene Fehlerart**, nicht `CertificateUntrusted` |

Die letzte Zeile ist der Kern: nur weil eine Pin-Abweichung von einem unbekannten
Zertifikat unterscheidbar ist, kann Stufe 1b beim einen nachfragen und beim anderen
hart abbrechen, wie Abschnitt 2.9 es verlangt.

Zwei Eigenschaften des Entwurfs wurden im Abschlussreview als baulich erzwungen
bestätigt statt nur beabsichtigt: das Zertifikat der Gegenstelle wird ausschließlich bei
`CertificateUntrusted` geerntet — im gepinnten Modus kann dieser Fall nicht auftreten,
eine erneute Nachfrage ist also unmöglich. Und eine Pin-Regel mit *leerem* Pin fällt auf
volle Prüfung zurück statt auf gar keine.

**Umgebung, die dabei sichtbar wurde:** `nas.example.com` löst auch im Heimnetz auf
die LAN-Adresse auf und liefert dort ein gültiges Zertifikat. Über die IP-Adresse
antwortet der Server nur über reines HTTP auf Port 8000. Der Client sollte deshalb
durchgängig über den Namen angesprochen werden — dann gilt Abschnitt 2.9 ohne Ausnahme.

---

### Stufe 1b — Pairing-Ablauf und Sitzung (abgenommen 2026-07-26)

122 Tests. Die Abnahme wurde gemeinsam mit dem Betreiber gegen den laufenden Server
gefahren, weil der entscheidende Schritt einen Menschen am Browser verlangt.

| Prüfung | Ergebnis |
|---|---|
| `pair "http://nas.example.com"` | abgelehnt **vor** jedem Netzkontakt |
| `pair "nas.example.com"` | Zertifikat regulär geprüft, Code angezeigt, nach Freigabe im Browser gepaart |
| Geräteliste der Web-Oberfläche | zeigt den Client als aktives Desktop-Gerät — der Server registriert selbst, ein eigener Aufruf entfällt |
| **`status` in einem neuen Prozess** | **verbunden, `/api/auth/me` → 200** |
| `session.json` | nur Adresse, Benutzername, Pin — **kein Token** |
| Anmeldeinformationsspeicher | beide Token getrennt unter `BaluDesk:session/…`, gerätegebunden |
| `logout` | Datei entfernt, Sitzung weg |
| `pair "nas.local"` | Fingerabdruck abgefragt — er stimmte **zeichengenau** mit dem aus der Abnahme der Stufe 1a überein |
| `status` danach | „Zertifikat: angeheftet", 200; der abgelegte Pin gleich dem aus 1a |

Die vierte Zeile ist das Abnahmekriterium aus Abschnitt 12: **ein Neustart führt ohne
erneutes Paaren zu einer authentifizierten Sitzung.**

Zwei Eigenschaften wurden dabei unabhängig doppelt belegt: Fingerabdruck und Pin des
Servers stimmten mit den Werten überein, die Stufe 1a auf einem anderen Weg ermittelt
hatte. Und die Ablage hält, was sie verspricht — die Token stehen nachweislich **nicht**
in der Datei.

Beim Abschlussreview kam ein Fehler ans Licht, der in derselben Stufe schon einmal
repariert worden war: der Rückbau sicherte zwar die Geheimnisse, aber das Öffnen der
Datei mit `trunc` zerstörte sie vorher. Behoben durch Schreiben in eine Nebendatei und
atomares Umbenennen — die bestehende Datei bleibt unangetastet, bis das Umbenennen
gelingt.

---

### Stufe 1c — Pairing-Oberfläche und Token-Erneuerung (abgenommen 2026-07-28)

195 Tests. Die Abnahme wurde gegen den laufenden Server gefahren, weil der
entscheidende Schritt einen Menschen am Browser verlangt.

| Prüfung | Ergebnis |
|---|---|
| `http://baluhost.duckdns.org` | sofort abgelehnt, ohne Netzkontakt: „Unverschlüsseltes http ist nur zu localhost erlaubt." |
| `ftp://baluhost.duckdns.org` | abgelehnt, eigene Meldung: „Nur http und https werden unterstützt, nicht „ftp"." |
| `baluhost.duckdns.org` | Codeanzeige mit Code, Link und Countdown, ohne Zertifikatsnachfrage |
| Fenster während der Codeanzeige auf Vollbild gezogen | reagierte flüssig, Layout hielt — der Arbeiterthread trägt |
| Im Browser angemeldet, Code eingegeben, freigegeben | Ansicht wechselte **selbsttätig** auf „Verbunden mit … als admin" |
| Geräteliste der Web-Oberfläche | `Sven-PC · Desktop · admin · active`, vom Server selbst registriert |
| **Beenden und neu starten** | **sofort verbunden, ohne Paaren** |
| `session.json` | nur Adresse, Benutzername, Pin — **kein Token** |
| Fünfmal gepaart, mit Abmelden dazwischen | **ein** Geräteeintrag, `Registered` blieb bei der ersten Registrierung |
| `baluhost.local` | Fingerabdruck mit konkretem Grund: `no alternative certificate subject name matches target hostname` |
| Fingerabdruck gegen Stufe 1b | zeichengenau gleich, zusätzlich mit openssl nachgerechnet |
| Bestätigt und freigegeben | gepaart; `session.json` trägt den Pin aus Stufe 1a und 1b |
| Ein Zeichen des Pins verfälscht, gestartet | Seite „Das Zertifikat hat sich geändert" mit `public key does not match pinned public key` |
| „Abmelden und neu verbinden" | `session.json` entfernt, zurück zur Eingabe |
| Auf der Zertifikatsseite „Abbrechen" | zurück zur Eingabe, **nichts angeheftet** |
| Während der Codeanzeige „Abbrechen" | zurück zur Eingabe |
| **Server angehalten, mit gültiger Sitzung gestartet** | **bleibt verbunden, Offline-Hinweis, verlangt kein Neupaaren** |
| Protokolldatei | „Sitzungsablage: …" nennt denselben Pfad wie `baludesk_probe` |

Damit ist das Abnahmekriterium aus Abschnitt 12 erstmals über die Anwendung
selbst erreicht statt über das Prüfprogramm.

Die vorletzte Zeile ist die wichtigste nach dem Neustart: Abschnitt 10 macht den
schlafenden Server zum Normalzustand. Die Anwendung hielt die verbundene Ansicht
samt Adresse und Benutzernamen und ergänzte nur einen Hinweis — bei einem echten
Zeitablauf von zehn Sekunden, nicht bei einem nachgestellten Fehler.

Das Zertifikat wurde auf drei unabhängigen Wegen bestätigt: die Anwendung zeigte
denselben Fingerabdruck, den openssl berechnete, und heftete denselben
öffentlichen Schlüssel an, den die Stufe-1b-Abnahme zwei Tage zuvor abgelegt
hatte. Bei der Pin-Abweichung wurde zum ersten Mal in der Oberfläche sichtbar,
was Stufe 1a als Kern belegt hatte: sie ist von einem unbekannten Zertifikat
unterscheidbar, und die Seite bietet folgerichtig **keinen** Knopf „trotzdem
verbinden".

Zwei Befunde kamen erst beim Gegenlesen der Reviews ans Licht, nicht aus den
Tests: curl wurde vor `QApplication` hochgefahren (Task 8, Runde 1), und der
schlafende Server zeigte die Eingabeseite statt der verbundenen Ansicht, weil
`serverUnreachable` Adresse und Benutzernamen nicht mittrug (Runde 2). Der
zweite Fall war vom Reviewer nur als toter Zweig gemeldet worden; dass die Spec
genau dieses Verhalten verlangt, fiel erst beim Abgleich auf.

**Der Abschluss-Review über den ganzen Zweig fand danach noch zwei Fehler, die
keinem der acht task-begrenzten Reviews auffallen konnten**, weil beide über
Task-Grenzen hinweg entstehen. `cancelPairing()` sendete ohne Wächter: ein
„Abbrechen"-Klick, der sich hinter einem laufenden `poll()` einreiht, warf die
Oberfläche auf die Eingabeseite, obwohl die Sitzung schon abgelegt war — die
Anwendung behauptete das Gegenteil dessen, was auf der Platte stand. Und der
Kaltstart zeigte bis zu 30 Sekunden die Eingabeseite, bevor `restoreSession()`
antwortete: derselbe Verstoß gegen Abschnitt 10 wie oben, nur transient und
deshalb bei der Abnahme von Hand nicht aufgefallen. Behoben durch Wächter und
eine eigene `StartupPage`; die Tests stiegen dabei von 179 auf 195.

Die Abnahme von Hand lief zunächst gegen den Stand vor diesen Behebungen. Weil
sie Verhalten auf geprüften Pfaden ändern, wurden die berührten Zeilen gegen den
Auslieferungsstand nachgefahren — einschließlich zweier neuer Belege: ein
Doppelklick auf „Vertrauen und fortfahren" erzeugt genau **einen** Gerätecode,
und bei angehaltenem Server steht während der Wartezeit „Verbindung wird
geprüft…" statt eines bedienbaren Eingabefelds.

---

## 15. Anhang: Zustand des Alt-Clients

Zur Einordnung, warum neu gebaut wird.

- Git-Repo auf `main`, 183 Commits, letzter vom 2026-04-06, zwei uncommittete Änderungen
- 187 getrackte Dateien; die großen Dateien im Arbeitsverzeichnis (233 MB `claude.exe`,
  100 MB `baludesk.db`, 81 MB WAL, ~15 MB Protokolle) sind ignoriert, das Repo selbst ist sauber
- Dublettenmuster durchgängig: `ipc_server.cpp` (16 KB) neben `ipc_server_fixed.cpp`
  (111 KB) neben `ipc_server_temp.cpp` neben `.backup`; ebenso bei `file_watcher`,
  `raid_info`, `FileExplorer.tsx`, `main.ts`
- `TODO.md` ist unzuverlässig: „Retry Logic mit Exponential Backoff" steht als offen,
  existiert aber als Template `retryWithBackoff()`. Beim Portieren gilt der Code, nicht das TODO.
