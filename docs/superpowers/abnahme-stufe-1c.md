# Abnahmeprotokoll Stufe 1c

Das ausführliche Protokoll zur Abnahme, die Abschnitt 14 der
[Spezifikation](specs/2026-07-26-baludesk-qt-design.md) zusammenfasst. Hier stehen
die Einzelbelege — wer nur das Ergebnis sucht, ist dort besser aufgehoben.

Gefahren am 2026-07-28 gemeinsam mit dem Betreiber gegen den laufenden BaluHost,
Debug-Build, `build/msvc2022/src/app/Debug/baludesk.exe`. 179 Tests grün vor Beginn;
die Nachprüfung am Ende lief gegen 195.

Ausgangslage: abgelegte Sitzung auf `https://baluhost.local` als `admin`,
Zertifikat angeheftet. Zugriffs- **und** Erneuerungstoken abgelaufen —
`AuthenticatedClient` unternahm genau einen Erneuerungsversuch
(`/api/auth/refresh` → 401) und meldete ihn als endgültig gescheitert.
Die Anwendung zeigte daraufhin die Eingabeseite mit „Die Sitzung ist
abgelaufen. Bitte neu verbinden." — Spec 2.3, Weg über `reauthRequired`.

| # | Prüfung | Ergebnis |
|---|---|---|
| 1 | `http://baluhost.duckdns.org` | ✅ sofort abgelehnt, ohne Netzkontakt — „Unverschlüsseltes http ist nur zu localhost erlaubt. Über diese Verbindung geht ein Zugangstoken — bitte https verwenden." |
| 2 | `ftp://baluhost.duckdns.org` | ✅ abgelehnt, eigene Meldung — „Nur http und https werden unterstützt, nicht „ftp"." |
| 3 | `baluhost.duckdns.org` | ✅ Codeanzeige mit Code, Link und Countdown, **keine** Zertifikatsnachfrage |
| 4 | Fenster während der Codeanzeige bewegen | ✅ auf Vollbild gezogen, während der Countdown lief — Layout hielt, keine Verzögerung |
| 5 | Im Browser anmelden, Code eingeben, freigeben | ✅ Ansicht wechselte **selbsttätig** auf „Verbunden mit https://baluhost.duckdns.org als admin" |
| 6 | Geräteliste der Web-Oberfläche | ✅ `Sven-PC · Desktop · admin · Registered 2m ago · Status active` |
| 7 | Beenden und neu starten | ✅ **sofort** „Verbunden mit https://baluhost.duckdns.org als admin", kein Code, kein Paaren |
| 8 | `session.json` ansehen | ✅ nur `server_url`, `username`, `public_key_pin` — kein Token; Pin leer, weil das duckdns-Zertifikat öffentlich vertrauenswürdig ist |
| 9 | Abmelden, neu paaren, Geräteliste | ✅ **derselbe** Eintrag `Sven-PC`, Total Devices unverändert 4 — zwei Paarungen (11:10:53, 11:13:37) mit Abmelden dazwischen erzeugten kein zweites Gerät |
| 10 | `baluhost.local` eingeben | ✅ Seite „Zertifikat des Servers bestätigen" mit Fingerabdruck und konkretem Grund: `SSL: no alternative certificate subject name matches target hostname 'baluhost.local'` |
| 11 | Fingerabdruck gegen Stufe 1b vergleichen | ✅ zeichengenau gleich über alle 32 Bytepaare: `00:4E:07:CA:8D:A4:F6:65:DD:12:F1:4D:04:25:1A:A8:92:B1:5B:2A:0D:71:92:CF:C1:40:F8:69:0D:5F:6F:7C` — unabhängig mit openssl nachgerechnet |
| 12 | Bestätigen und freigeben | ✅ gepaart; `public_key_pin` = `sha256//Gl2pcsze68ssKgONvsRilq9788PJ36LyWPUlHFJ7DCk=`, gleich dem openssl-Wert und dem aus Stufe 1b |
| 13 | Pin verfälschen, starten | ✅ Seite „Das Zertifikat hat sich geändert" mit `SSL: public key does not match pinned public key` — die **eigene** Fehlerart für Pin-Abweichung, kein stiller Abbruch, kein Knopf „trotzdem verbinden" |
| 14 | „Abmelden und neu verbinden" | ✅ `session.json` **vollständig entfernt**, zurück zur Eingabe der Serveradresse ohne stehengebliebene Meldung |
| 15 | Auf der Zertifikatsseite „Abbrechen" | ✅ zurück zur Eingabe; **nichts angeheftet** — im Protokoll fehlt `Vertrauen: Zertifikat bestätigt und Schlüssel angeheftet`, `session.json` blieb ohne Pin |
| 16 | Während der Codeanzeige „Abbrechen" | ✅ zurück zur Eingabe, danach reguläre Paarung möglich |
| 17 | Server anhalten, mit gültiger Sitzung starten | ✅ bleibt auf „Verbunden mit https://baluhost.duckdns.org als admin" und ergänzt „Der Server ist zurzeit nicht erreichbar." — Protokoll: `Connection timed out after 10015 milliseconds`. Kein Neupaaren verlangt. |
| 18 | Protokolldatei gegen `baludesk_probe` | ✅ beide `…\BaluHost\BaluDesk\session.json` |

**Abweichung von der Plan-Reihenfolge.** Zeile 17 wurde vor 13/14 gefahren. Zeile 14
entfernt die Sitzung; stur der Reihe nach hätte der Auftraggeber für 17 ein weiteres
Mal komplett paaren müssen. Die beiden Zeilen prüfen Unabhängiges — 17 den nicht
erreichbaren Server bei gültiger Sitzung, 13 den verfälschten Pin bei erreichbarem
Server — die Reihenfolge ändert nichts am Belegten. Beim Auftraggeber vorher angezeigt.

**Zwei unabhängige Wege beim Zertifikat.** Der Fingerabdruck der Anwendung und der
mit openssl berechnete stimmen zeichengenau überein, ebenso der angeheftete
öffentliche Schlüssel und der, den die Stufe-1b-Abnahme zwei Tage zuvor abgelegt
hatte. Drei Ermittlungswege, ein Ergebnis.

**Fünf Paarungen, ein Geräteeintrag.** Über die ganze Abnahme hinweg wurde fünfmal
gepaart, mit Abmelden dazwischen. Die Geräteliste zeigte durchgehend `Sven-PC`
einmal, `Registered` blieb bei der ersten Registrierung um 11:10 stehen.

**Zustand nach der Abnahme:** keine Sitzung — Zeile 14 hat sie entfernt.

---

## Nachprüfung gegen den Merge-Kandidaten (f18877a)

Die Abnahme oben lief gegen `a7a734c`. Der Abschluss-Review brachte danach eine
Fix-Runde (179 → 195 Tests), die Verhalten auf geprüften Pfaden änderte. Die
berührten Zeilen wurden deshalb gegen `f18877a` nachgefahren — auf Entscheidung
des Auftraggebers, statt die Abnahme auf einem überholten Stand stehen zu lassen.

| # | Prüfung | Ergebnis |
|---|---|---|
| 7 | Beenden und neu starten | ✅ sofort „Verbunden mit https://baluhost.local als admin" |
| 10 | `baluhost.local` eingeben | ✅ Zertifikatsseite, Fingerabdruck unverändert `00:4E:07:…:5F:6F:7C` |
| 12 | Bestätigen und freigeben | ✅ gepaart; **Doppelklick auf „Vertrauen und fortfahren" erzeugte genau einen** Gerätecode — die neue Sperre trägt |
| 13 | Pin verfälschen, starten | ✅ „Das Zertifikat hat sich geändert", Fassung für den **angehefteten** Pin, `public key does not match pinned public key` |
| 14 | „Abmelden und neu verbinden" | ✅ `session.json` entfernt, zurück zur Eingabe |
| 15 | Auf der Zertifikatsseite „Abbrechen" | ✅ zurück zur Eingabe |
| 17 | Server angehalten, mit gültiger Sitzung gestartet | ✅ **„Verbindung wird geprüft…" während der Wartezeit**, danach verbundene Ansicht mit Offline-Hinweis |

Zeile 17 ist der Beleg für Befund 2 des Abschluss-Reviews: wo vorher zehn Sekunden
lang ein bedienbares Eingabefeld stand und zum Neupaaren einlud, steht jetzt eine
Startseite ohne Feld und ohne Knopf.

Zeile 12 ist der Beleg für Befund 3: das Protokoll zeigt genau ein
`Vertrauen: Zertifikat bestätigt` und genau ein `Pairing: Code ausgestellt`,
obwohl zweimal geklickt wurde.

**Nicht von Hand geprüft:** die zweite Textfassung der Seite „Das Zertifikat hat
sich geändert" — der Fall „abgelaufenes Zertifikat, nie etwas angeheftet" lässt
sich ohne ein wirklich ablaufendes Zertifikat nicht herstellen. Gedeckt durch
`DoesNotClaimAnExchangeWhenNothingWasEverPinned`.

**Nicht erneut geprüft, weil unberührt von der Fix-Runde:** Zeilen 1-6, 8, 9, 11,
16, 18. Kein Commit der Runde fasst `ServerUrl`, die Sitzungsablage, die
Gerätekennung oder den Logger an.
