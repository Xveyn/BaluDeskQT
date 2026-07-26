# Übergabe an Stufe 1b

Stand nach Abnahme der Stufe 1a am 2026-07-26, Commit `2143afb`. 60 Tests grün.

Was hier steht, ist absichtlich **nicht** im Plan der Stufe 1a gelandet: es sind Punkte,
die dort bewusst offengelassen wurden, plus Risiken, die die Reviews für die nächste
Stufe benannt haben. Beim Schreiben des Plans für Stufe 1b ist das die Liste, die man
zuerst durchgeht.

---

## Vor dem ersten Code zu klären

**Der Poll-Ablauf hat keine Reserve.** `/api/desktop-pairing/poll` ist auf 12 Anfragen je
Minute begrenzt, und der Server gibt selbst ein Intervall von 5 Sekunden vor — das ist
exakt ein Abruf je Intervall. Ein Client, der bei `authorization_pending` sofort erneut
fragt, sperrt sich aus. Das vom Server gelieferte `interval` ist einzuhalten, nicht zu
unterbieten.

**`/api/auth/refresh` rotiert das Erneuerungstoken nicht.** Läuft es ab, hilft kein
Erneuern mehr — dann muss neu gepaart werden. Der Ablauf muss diesen Fall von einem
gewöhnlichen 401 unterscheiden, sonst landet der Nutzer in einer Schleife.

**Antwortkopfzeilen sind nicht auslesbar.** `HttpResponse` trägt nur Status und Rumpf,
`CurlTransport` setzt keine `CURLOPT_HEADERFUNCTION`. Signalisiert der Server bei 429 ein
`Retry-After`, kommt man derzeit nicht heran. Ein Feld in der Struktur zu ergänzen ist
quellkompatibel — aber den Serververtrag früh prüfen, nicht mitten im Ablauf entdecken.

**Windows deckelt ein Geheimnis bei `CRED_MAX_CREDENTIAL_BLOB_SIZE` (2560 Bytes).**
`WindowsCredentialStore::save` prüft das jetzt und meldet es mit Größe und Grenze im
Protokoll, statt einen nackten Fehlercode zu liefern. Ein Zugangs- und ein
Erneuerungstoken zusammen können in die Nähe kommen — getrennt ablegen, nicht als ein
Feld.

**Kein Ort erzwingt „HTTP nur für localhost".** Spec Abschnitt 2.9 verlangt es,
`HttpClient` nimmt aber jede Basis-URL und `CurlTransport` schickt bereitwillig ein
`Authorization: Bearer …` über blankes `http://`. Stufe 1b ist die Stelle, an der ein
Mensch zum ersten Mal eine Adresse eintippt. Die Prüfung gehört nach `core/api/` neben
den `HttpClient` — testbar und für jeden Aufrufer gültig —, nicht allein in die
Eingabeprüfung des Dialogs, die der nächste Einstiegspunkt umgeht.

**Nichts bindet einen Pin an eine Nutzerentscheidung.** `TlsPolicy` ist ein offenes
Aggregat; der Pin wird in Stufe 1b aus den Einstellungen gelesen. Eine benannte
Erzeugerfunktion — etwa `TlsPolicy::pinnedFromConfirmedCertificate(...)` — oder wenigstens
ein Kommentar an der Lese- und Schreibstelle hält fest, dass dieser Wert nur aus einem
abgeschlossenen Bestätigungsdialog stammen darf. Der Fehlerfall ist immerhin gutartig:
ein Pin, der nicht passt, scheitert geschlossen mit `CertificatePinMismatch`, statt
unsicher zu verbinden.

## Eigenschaften, auf die man sich verlassen kann

Vom Abschlussreview als **baulich erzwungen** bestätigt, nicht bloß beabsichtigt:

- Das Zertifikat der Gegenstelle wird ausschließlich bei `CertificateUntrusted` geerntet.
  Im gepinnten Modus kann dieser Fall nicht auftreten — eine erneute Nachfrage nach einer
  Pin-Abweichung ist damit unmöglich, selbst wenn der Dialog nachlässig verdrahtet wird.
- Eine Pin-Regel mit **leerem** Pin fällt auf volle Prüfung zurück, nicht auf gar keine.
- `CurlTransport` ist jetzt gefahrlos mehrfach erzeugbar: `curl_global_init` läuft einmal
  über `std::call_once`, `curl_global_cleanup` entfällt. Trotzdem besser eine Instanz beim
  Programmstart anlegen und weiterreichen, als eine je Dialog zu bauen.
- `HttpClient::setTlsPolicy` ist gegen `send` synchronisiert. Der Bestätigungsdialog darf
  die Regel also aus dem GUI-Thread setzen, während der Poll-Ablauf auf einem anderen
  Thread läuft.

## Werkzeuge, die schon da sind

- **`tools/probe`** — Prüfprogramm gegen einen echten Server, hinter der Option
  `BALUDESK_BUILD_TOOLS`. Damit lassen sich die Pairing-Endpunkte ausprobieren, bevor ein
  Dialog existiert.
- **`tests/core/support/FakeTransport.h`** — mit einer **Warteschlange** von Antworten.
  Aufeinanderfolgende Aufrufe liefern aufeinanderfolgende Ergebnisse, die letzte wiederholt
  sich. Genau dafür gebaut, den Ablauf `authorization_pending` → `authorization_pending` →
  `approved` zu prüfen.
- **`core/util/SecretStore.h`** — Schnittstelle über dem Anmeldeinformationsspeicher, damit
  die Sitzungslogik gegen einen Fake geprüft werden kann statt gegen echte Zugangsdaten.

## Zurückgestellte Kleinigkeiten

Keine davon blockiert etwas; beim nächsten Anfassen der jeweiligen Datei mitnehmen.

- `tests/app/qt_log_bridge_test.cpp`: totes `#include <filesystem>` samt ungenutztem Alias
- Zuordnung von `QtDebugMsg`, `QtInfoMsg` und `QtFatalMsg` ungetestet
- `TlsTrust.cpp`: `rawSpki` wird erst nach der Fehlerprüfung in seinen RAII-Halter gefasst
- `RejectsATruncatedCertificate` entfernt den PEM-Fuß, prüft also „Fuß fehlt" statt
  „DER mittendrin abgeschnitten"
- Der Nullblob-Schutz in `load()` hat keinen eigenen Test
- `curl_slist_append` schreibt in denselben Rohzeiger zurück — bei Allokationsfehler wäre
  die bisherige Liste unerreichbar
- `TransportResult` wird in `HttpClient::send` als Ganzes kopiert, einschließlich Rumpf
- `CURLOPT_MAXFILESIZE` von 8 MiB ist gesetzt, aber von keinem Test mit einer zu großen
  Übertragung belegt
- `windeployqt` meldet beim Bauen `VCINSTALLDIR is not set` und kopiert deshalb die
  MSVC-Laufzeitbibliotheken nicht mit — lokal folgenlos, relevant für Stufe 7
- Kein `compile_commands.json` (der VS-Generator erzeugt keins), IntelliSense löst die
  Includes deshalb nicht auf
- Die Version steht an drei Stellen (`main.cpp`, `CMakeLists.txt`, `vcpkg.json`)
- Kein `qInstallMessageHandler` für Qts eigene Meldungen fehlt nicht mehr — das ist erledigt

## Eine bekannte, nicht geschlossene Lücke

`baludesk_core` linkt curl und OpenSSL als `PRIVATE`, damit deren Include-Pfade nicht nach
`src/app/` durchschlagen. **Das schließt die Lücke nicht.** vcpkg legt alle Pakete in ein
gemeinsames `include/`-Verzeichnis, und `spdlog` muss öffentlich bleiben, weil es in
`Logger.h` vorkommt — damit erreicht dieses Verzeichnis `src/app/` weiterhin, und ein
`#include <curl/curl.h>` dort ließe sich übersetzen. Die `PRIVATE`-Angabe ist trotzdem
richtig, weil sie die Absicht im Bausystem festhält; ein wirklicher Riegel bräuchte
getrennte Install-Wurzeln je Bibliothek. Nachzulesen im Kommentar an
`src/core/CMakeLists.txt`.

Praktisch heißt das: dass niemand curl direkt aus `src/app/` aufruft und damit die
Transport-Naht umgeht, ist derzeit Disziplin, nicht Mechanik.
