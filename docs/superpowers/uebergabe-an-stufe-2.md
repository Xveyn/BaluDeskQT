# Übergabe an Stufe 2

Stand nach Abnahme der Stufe 1c am 2026-07-28, Code auf Commit `bb1655c`. 179 Tests grün.
Ersetzt `uebergabe-an-stufe-1c.md`, dessen Punkte entweder erledigt sind oder hier
weitergeführt werden.

Stufe 1 ist abgeschlossen: die Anwendung paart sich selbst, hält die Sitzung über
Neustarts, erneuert Token und übersteht einen schlafenden Server. Stufe 2 bringt die
Synchronisation — und damit den ersten Aufrufer, der die Sitzung *benutzt*, statt sie
nur herzustellen.

---

## Was fertig dasteht und benutzt werden will

| Baustein | Wofür |
|---|---|
| `core/auth/AuthenticatedClient` | **Jeder Sync-Aufruf gehört hindurch, nicht an `HttpClient` vorbei.** Er hängt das Zugangstoken an, erkennt 401, erneuert **genau einmal** und legt das neue Token ab. Wer `HttpClient` direkt nimmt, baut die Erneuerung ein zweites Mal nach — schlechter. |
| `core/auth/AuthOutcome` | Fünf Werte, die den Unterschied tragen, an dem sich ein Client sonst im Kreis dreht: erneuerbar, endgültig abgelaufen, Server nicht erreichbar, Zertifikat verändert, Ok. |
| `bridge/AuthBridge` | Das Muster für `SyncEngineBridge`: `QObject` im Arbeiterthread, blockierende `core`-Aufrufe in Slots, Ergebnisse als Signale. Nachbauen, nicht neu erfinden. |
| `core/api/TrustNegotiation` | Die Vertrauensentscheidung als Zustandsobjekt ohne Ein-/Ausgabe — `check()`, `confirm()`, `reject()`. In Stufe 1c aus `tools/probe` herausgezogen; jeder weitere Aufrufer erbt damit die Tests. |
| `core/api/checkServerUrl(input)` | Adressprüfung samt Normalisierung. Gilt für jeden Aufrufer. |
| `app/SettingsStore::deviceId()` | Erzeugt die Gerätekennung einmal und behält sie. **Überlebt das Abmelden** — die Abnahme hat mit fünf Paarungen belegt, dass der Server daraus einen einzigen Geräteeintrag macht. Wer eine eigene Kennung erfindet, erzeugt Karteileichen. |
| `core/auth/FileSessionRepository` | Ablegen, laden, entfernen; schreibt in eine Nebendatei und benennt atomar um. `saveAccessToken()` für den Erneuerungspfad allein. |
| `core/auth/defaultSessionPath()` | Die einzige Quelle für den Ablageort. |
| `tests/core/support/` | `FakeTransport` (Antwortwarteschlange), `FakeSecretStore`, `FakeSessionRepository`. Aus `tests/bridge/` und `tests/app/` als `"core/support/…"` einbinden. |
| `tools/probe` | Fährt Paaren, Status und Abmelden von der Kommandozeile — zum Nachsehen, ohne die Oberfläche zu bemühen. |

## Fallen, die schon jemanden gekostet hätten

**`resetPairing()` löscht `pendingServerUrl_`.** `connectToServer()` ruft es als
allererstes auf (`AuthBridge.cpp:133`). Wer Adresse oder Benutzername nach dem Aufruf
noch braucht — etwa um ein Signal damit zu bestücken — muss sie **vorher** sichern.
Genau daran ist die erste Fassung des schlafenden Servers gescheitert:
`serverUnreachable` trug die Adresse nicht mit, also konnte die Oberfläche die
verbundene Ansicht nicht aufbauen und fiel auf die Eingabeseite zurück.

**`baludesk_bridge` darf `Qt6::Widgets` nicht linken.** Es linkt bewusst nur
`Qt6::Core` (`src/bridge/CMakeLists.txt:18`). Der Grund steht als Kommentar darüber:
läge die Brücke bei `baludesk_app_lib`, könnte jederzeit ein `QMessageBox` in den
Arbeiterthread rutschen. Eine Bibliothek ohne Widgets macht daraus einen Übersetzungs-
statt eines Laufzeitfehlers. **Diese Grenze nicht aufweichen**, wenn `SyncEngineBridge`
dazukommt.

**Die Deklarationsreihenfolge von `secrets_` und `repository_` in `AuthBridge` ist
tragend** (`AuthBridge.h:130-131`). `FileSessionRepository` hält eine `SecretStore&`.
Mitglieder sterben in umgekehrter Deklarationsreihenfolge, also stirbt `repository_`
vor `secrets_`. Wer die beiden Zeilen tauscht, baut eine Referenz auf ein zerstörtes
Objekt — still, und erst beim Beenden.

**`HttpClient` blockiert bis zu 30 Sekunden und lässt sich nicht abbrechen**
(Spec 2.10, bewusst so). Der Aufruf gehört **nicht** in den GUI-Thread.

**Die Token werden genau einmal geliefert.** Der Server löscht seinen Datensatz beim
Ausliefern und antwortet danach mit 404. Zwischen `PairingState::Approved` und
`repository.save()` darf nichts stehen, was scheitern kann.

**curl muss vor `QApplication` hochgefahren werden.** `curl_global_init` ist nicht
threadsicher; läuft es erst, wenn Qt schon Threads hat, ist das ein Wettlauf. In
`main()` steht die Reihenfolge deshalb fest.

**Der Logger schreibt Info-Zeilen nicht sofort in die Datei.**
`flush_on(spdlog::level::warn)` — bei einem Lauf ohne Warnung stehen „gestartet" und
„Sitzungsablage: …" bis zum Beenden im Puffer. Wer beim Fehlersuchen in eine noch
laufende Anwendung schaut und die Zeilen vermisst, sucht am falschen Ende.

## Was Stufe 2 neu entscheiden muss

**Ein Thread oder zwei?** Spec 7.1 nennt Koordinator- und Transfer-Threads getrennt.
Stufe 1c hat genau einen Arbeiterthread, in dem `AuthBridge` lebt. Ob der Sync-Thread
derselbe ist oder ein eigener, ist offen — und die Antwort bestimmt, ob der
`AuthenticatedClient` überhaupt zwischen Threads wandern muss.

**Wie gehen `authRequired` (Spec 6.3) und `reauthRequired` (`AuthBridge`) zusammen?**
Zwei Namen für vermutlich denselben Zustand, aus zwei Schichten. Bevor der zweite
Aufrufer entsteht, gehört entschieden, wer wen auslöst — sonst hat die Oberfläche zwei
Wege zur selben Seite und keiner ist der maßgebliche.

**Wem gehört der `AuthenticatedClient`, wenn zwei Brücken ihn brauchen?** Er hält
`HttpClient&` und `SessionRepository&` und trägt die `Session` als Wert, den er beim
Erneuern fortschreibt. Zwei Instanzen mit je eigener Kopie der Sitzung erneuern
gegeneinander. Eigentümerschaft festlegen, bevor `SyncEngineBridge` entsteht.

**Wer bemerkt, dass der Server wieder wach ist?** Heute prüft nichts periodisch nach;
der Offline-Hinweis bleibt stehen, bis der Benutzer etwas tut. Sobald Stufe 2 eine
periodische Prüfung bringt, wird aus dem Hinweis ein Zustand mit Rückweg — und die
unten genannte Falle mit dem weiterlaufenden Countdown wird echt.

## Bekannte offene Punkte

Keiner blockiert; beim nächsten Anfassen der jeweiligen Datei mitnehmen.

**Aus Stufe 1c, in den Reviews aufgelaufen und bewusst zurückgestellt:**

- `~MainWindow` kann bis zu 30 s in `wait()` stehen, und `Logger::shutdown()` läuft in
  `main()` davor. Was beim Aufräumen schiefgeht, verpufft still. Heute latent, mit
  einem zweiten Arbeiterthread nicht mehr.
- Fünf Handler wechseln auf die Serveradresse-Seite, ohne `pairingCodePage_->stop()`
  zu rufen. Heute unerreichbar; sobald eine periodische Sitzungsprüfung dazukommt,
  läuft der Countdown weiter. Strukturell zu schließen mit einem
  `showPage(QWidget*)`-Helfer, der das Anhalten mitnimmt.
- Auf der Zertifikatsseite ist kein Knopf gesperrt, während `confirm()` läuft — ein
  zweiter Klick erzeugt einen zweiten Gerätecode.
- Der `PinMismatch`-Zweig in `connectToServer()` ruft als einziger kein
  `resetPairing()`. Folgenlos, weil der Zweig aus einer frischen Verbindung mit
  SystemTrust nicht erreichbar ist und jeder neue Versuch ohnehin damit beginnt.
- `confirmCertificate()` endet bei `trust_ == nullptr` ohne Signal. Über die
  Oberfläche nicht erreichbar, weil der Knopf erst nach
  `certificateConfirmationRequired` erscheint.
- `FakeSessionRepository` trägt drei Felder, die kein Test benutzt: `failSave`,
  `failClear`, `clearCallCount`. Entfernen oder durch einen Test rechtfertigen.
- Kein Test setzt `failSaveAccessToken = true`. Der bewusst gewählte Pfad „Ablegen
  scheitert, der Aufruf läuft trotzdem weiter" ist damit nur im Code dokumentiert,
  nicht festgenagelt.
- Die Fehlschalter der Testhilfen (`failOnSaveNumber`, `failOnEraseNumber`) zählen
  Aufrufe global statt pfadbezogen. Verschöbe eine künftige Änderung die Zahl der
  `secrets_.save()`-Aufrufe, träfe der Test einen anderen Aufruf, ohne rot zu werden.

**Aus Stufe 1b übernommen, weiterhin offen:**

- `PairingClient`: fehlendes `user`-Objekt bei `approved` wird stillschweigend
  hingenommen, `username` bleibt leer.
- `PairingClient`: kein Test für einen unerwarteten HTTP-Status beim Abruf.
- `intField` verlangt eine ganze Zahl — ein Server, der `"interval": 5.0` schickt,
  fällt still auf den Vorgabewert zurück.
- `windeployqt` meldet `VCINSTALLDIR is not set` und kopiert die
  MSVC-Laufzeitbibliotheken nicht mit. Relevant erst für Stufe 7.
- Kein `compile_commands.json` — IntelliSense löst die Includes nicht auf.
- Die Version steht an drei Stellen (`main.cpp`, `CMakeLists.txt`, `vcpkg.json`).
- `baludesk_core` linkt curl und OpenSSL als `PRIVATE`, was das Include-Leck nach
  `src/app/` **nicht** schließt: vcpkg legt alle Pakete in ein gemeinsames Verzeichnis
  und `spdlog` muss öffentlich bleiben. Dass niemand curl direkt aus `src/app/`
  aufruft und damit die Transport-Naht umgeht, ist Disziplin, nicht Mechanik.

**Erledigt in Stufe 1c und deshalb nicht mehr aufgeführt:** die Extraktion der
Vertrauensentscheidung, der fehlende Weg zurück bei veraltetem Pin, das Zeitfenster
beim Speichern, `defaultSessionPath()` mit `"."`, die nicht ausgewerteten
`std::error_code`, `clear()` mit `void`, die `SecretStore&`-Lebensdauer.
