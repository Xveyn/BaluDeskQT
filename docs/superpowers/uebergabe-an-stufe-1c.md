# Übergabe an Stufe 1c

Stand nach Abnahme der Stufe 1b am 2026-07-26, Commit `09858b5`. 122 Tests grün.
Ersetzt `uebergabe-an-stufe-1b.md`, dessen Punkte entweder erledigt sind oder hier
weitergeführt werden.

Stufe 1c bringt die Oberfläche: Server-URL eingeben, Zertifikat bestätigen, Code
anzeigen, Einbindung ins Hauptfenster, Abmelden — und die Token-Erneuerung.

---

## Was fertig dasteht und benutzt werden will

| Baustein | Wofür |
|---|---|
| `core/api/checkServerUrl(input)` | Adressprüfung samt Normalisierung. **Nicht im Dialog nachbauen** — die Regel gilt für jeden Aufrufer, und der Dialog wäre nur einer davon. |
| `core/api/TlsPolicy::pinnedFromConfirmedCertificate(pin)` | Der Name ist der Vertrag: der Wert darf nur aus einem abgeschlossenen Bestätigungsvorgang stammen. Leerer Pin ergibt bewusst wieder volle Prüfung. |
| `core/util/identifyCertificate(pem)` | Fingerabdruck zum Anzeigen, Pin zum Anheften |
| `core/auth/PairingClient` | Der Ablauf ohne eigenes Zeitgefühl — genau dafür gebaut, an einem `QTimer` zu hängen |
| `core/auth/FileSessionRepository` | Sitzung ablegen, laden, entfernen |
| `core/auth/defaultSessionPath()` | **Die einzige Quelle für den Ablageort.** Siehe Warnung unten. |
| `tests/core/support/FakeTransport.h` | Warteschlange von Antworten, für Poll-Folgen |
| `tests/core/support/FakeSecretStore.h` | Geheimnisse im Speicher, `failOnSaveNumber` lässt den n-ten Aufruf scheitern |
| `tools/probe` | Fährt den ganzen Ablauf von der Kommandozeile — zum Ausprobieren, bevor ein Dialog existiert |

## Fallen, die schon jemanden gekostet hätten

**`QStandardPaths::AppDataLocation` zeigt auf Roaming, nicht auf Local.** Der naheliegende
Qt-Aufruf liefert ein *anderes* Verzeichnis als `defaultSessionPath()`. Der Fehlermodus
ist still: ein korrekt gepaarter Client meldet, er sei nicht gepaart. Benutze
`defaultSessionPath()`, nicht `QStandardPaths`.

**`poll()` gehört an einen Zeitgeber, nicht in eine Schleife.** Der `PairingClient` gibt
`waitSeconds` zurück, damit genau das möglich ist. Das Prüfprogramm schläft nur, weil es
einen einzigen Ablauf fährt und nichts anderes zu tun hat.

**Das vom Server gelieferte Intervall ist einzuhalten.** 12 Abrufe je Minute bei
vorgegebenen 5 Sekunden — keine Reserve. Der `PairingClient` hält das ein; wer ihn
umgeht, sperrt den Client aus.

**Die Token werden genau einmal geliefert.** Der Server löscht seinen Datensatz beim
Ausliefern und antwortet danach mit 404. Zwischen `PairingState::Approved` und
`repository.save()` darf nichts stehen, was scheitern kann.

**`HttpClient` blockiert bis zu 30 Sekunden und lässt sich nicht abbrechen**
(Spec 2.10, bewusst so entschieden). Der Aufruf gehört deshalb **nicht** in den
GUI-Thread — sonst friert das Fenster ein. `setTlsPolicy` ist gegen `send`
synchronisiert, der Bestätigungsdialog darf die Regel also aus dem GUI-Thread setzen.

## Was Stufe 1c neu entscheiden muss

**Die Vertrauensentscheidung hat keine Naht in `core/`.** Der Ablauf „nicht
vertrauenswürdig → Fingerabdruck zeigen → bestätigen → anheften → erneut prüfen" steckt
in `tools/probe/main.cpp` und ist mit `std::cin`/`std::cout` verwachsen. Der
`PairingClient` bekam die ein-/ausgabefreie Behandlung genau deshalb; dieser Ablauf nicht.
Wer den Dialog schreibt, baut ihn sonst von Null nach, ohne Tests zu erben. **Vorher als
kleines Zustandsobjekt herausziehen.**

**Für einen veralteten Pin gibt es keinen Weg zurück.** Wechselt der Server von
selbstsigniert auf ein gültiges Zertifikat, passt der gespeicherte Pin nicht mehr und die
Verbindung bricht hart ab — richtig laut Spec 2.9, aber ohne Erklärung und ohne Angebot,
neu zu bestätigen. `Session` trägt einen Pin, kennt aber kein Verwerfen. Das braucht eine
bewusste Antwort, bevor jemand im Feld davorsteht.

**Token-Erneuerung läuft über `SessionRepository::save()`** — bisher nur für frisches
Paaren benutzt. `POST /api/auth/refresh` mit dem Erneuerungstoken; es wird **nicht
rotiert**. Läuft es ab, hilft kein Erneuern, dann muss neu gepaart werden. Dieser Fall ist
von einem gewöhnlichen 401 zu unterscheiden, sonst dreht sich der Client im Kreis.

**`FileSessionRepository` hält eine `SecretStore&`.** Im Prüfprogramm unkritisch, weil die
Reihenfolge der Deklarationen es regelt. In einer Brückenschicht mit langlebigen
Mitgliedern ist eine solche Referenz die Art Sache, die still ihren Bezug überlebt.
Eigentümerschaft bewusst festlegen.

## Bekannte offene Punkte

Keiner blockiert; beim nächsten Anfassen der jeweiligen Datei mitnehmen.

- **Zeitfenster beim Speichern:** `save()` legt erst die Geheimnisse ab, dann die Datei.
  Bei einem harten Absturz genau dazwischen stehen neue Token neben alten Metadaten.
  Vorher schon so, aber die Token-Erneuerung fährt diesen Pfad künftig ständig.
- `defaultSessionPath()` weicht auf `"."` aus, wenn `LOCALAPPDATA` fehlt — das verletzt
  die Zusicherung „immer absolut", die der eigene Header gibt.
- `std::error_code` von `create_directories`, `remove` und `rename` wird nicht ausgewertet
  — ein Verzeichnisfehler ist im Protokoll nicht von einem Dateifehler zu unterscheiden.
- `SessionRepository::clear()` gibt `void` zurück und verschluckt beide `erase()`-
  Ergebnisse. Der Abmelde-Knopf kann deshalb nur „erledigt" sagen, auch wenn ein Token
  im Speicher zurückblieb.
- `PairingClient`: fehlendes `user`-Objekt bei `approved` wird stillschweigend
  hingenommen, `username` bleibt leer. Vermutlich richtig, aber nicht dokumentiert.
- `PairingClient`: kein Test für einen unerwarteten HTTP-Status beim Abruf.
- `intField` verlangt eine ganze Zahl — ein Server, der `"interval": 5.0` schickt, fällt
  still auf den Vorgabewert zurück.
- `windeployqt` meldet `VCINSTALLDIR is not set` und kopiert deshalb die
  MSVC-Laufzeitbibliotheken nicht mit. Relevant erst für Stufe 7.
- Kein `compile_commands.json` — IntelliSense löst die Includes nicht auf.
- Die Version steht an drei Stellen (`main.cpp`, `CMakeLists.txt`, `vcpkg.json`).
- `baludesk_core` linkt curl und OpenSSL als `PRIVATE`, was das Include-Leck nach
  `src/app/` **nicht** schließt: vcpkg legt alle Pakete in ein gemeinsames Verzeichnis
  und `spdlog` muss öffentlich bleiben. Dass niemand curl direkt aus `src/app/` aufruft
  und damit die Transport-Naht umgeht, ist derzeit Disziplin, nicht Mechanik.
