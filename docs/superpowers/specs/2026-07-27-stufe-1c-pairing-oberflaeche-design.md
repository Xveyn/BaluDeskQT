# BaluDesk QT — Stufe 1c (Pairing-Oberfläche und Token-Erneuerung) Design

**Datum:** 2026-07-27
**Status:** Entwurf, vom Auftraggeber freigegeben
**Übergeordnete Spec:** `docs/superpowers/specs/2026-07-26-baludesk-qt-design.md` — Abschnitte 2.6, 2.9, 2.10, 5, 12 (Stufe 1)
**Übergabe aus 1b:** `docs/superpowers/uebergabe-an-stufe-1c.md`
**Vorgänger:** `docs/superpowers/plans/2026-07-26-stufe-1b-pairing-und-sitzung.md`, abgenommen, 122 Tests grün

---

## 1. Zweck

Stufe 1b hat den Paarungsablauf gebaut und über die Kommandozeile belegt. Stufe 1c gibt
ihm eine Oberfläche und macht die Sitzung haltbar: Adresse eingeben, Zertifikat
bestätigen, Code anzeigen, verbunden bleiben, abmelden — und das Zugangstoken erneuern,
bevor es jemandem auffällt.

Damit ist das Abnahmekriterium der Stufe 1 aus Abschnitt 12 der übergeordneten Spec
erstmals über die Anwendung selbst erreichbar statt über ein Prüfprogramm.

Stufe 1c ist außerdem die erste Stufe, in der alle drei Schichten aus Abschnitt 5 besetzt
sind. Wie die Naht zwischen ihnen hier gezogen wird, gilt für alles Weitere.

---

## 2. Getroffene Entscheidungen

### 2.1 Ansichtswechsel im Hauptfenster statt modalem Assistenten

`MainWindow` bekommt einen `QStackedWidget`. Ist keine Sitzung abgelegt, steht die
Paarungsansicht darin, sonst die verbundene Ansicht. Die drei Paarungsschritte sind Seiten
desselben Stapels, keine Dialoge.

Begründung: Das Fenster wächst in den Stufen 2 bis 6 um Sync-Ansicht, Konfliktliste,
Dashboard und Explorer — allesamt weitere Seiten desselben Stapels. Ein modaler Assistent
über einem leeren, gesperrten Fenster wäre für den Erstkontakt gebaut und müsste beim
Abmelden und beim Signal `authRequired` (Spec 6.3) erneut aufgerufen werden. Der Stapel
kennt diesen Sonderfall nicht: „nicht angemeldet" ist bei ihm einfach eine Seite.

Erwogen und verworfen: modaler Paarungs-Assistent; drei getrennte `QDialog`, von einem
Controller nacheinander aufgerufen. Letzteres läge am nächsten am Prüfprogramm, verteilt
die Ablaufreihenfolge aber auf einen Controller, den es sonst nirgends gibt.

### 2.2 Ein eigener Arbeiterthread in `bridge/`

`AuthBridge : QObject` besitzt `HttpClient`, `TrustNegotiation`, `PairingClient`,
`AuthenticatedClient` und das `SessionRepository` und lebt per `moveToThread` in einem
eigenen `QThread`. Die Oberfläche redet ausschließlich über Signale und Slots mit ihr.

Begründung: `HttpClient` blockiert bis zu 30 Sekunden und lässt sich nicht abbrechen
(Spec 2.10, bewusst so entschieden). Im GUI-Thread friert damit genau das Fenster ein, das
gerade „warte auf Freigabe" anzeigt. Spec Abschnitt 5 sieht `bridge/` für exakt diesen
Zweck vor.

Der zweite Gewinn ist Eigentümerschaft: liegt der gesamte Zustand in einem Objekt in einem
Thread, gibt es keine geteilten Daten, die zu sichern wären. Die Übergabe warnt, dass
`FileSessionRepository` eine `SecretStore&` hält — in der Bridge halten beide dieselbe
Lebensdauer wie die Bridge selbst, in der richtigen Deklarationsreihenfolge.

Erwogen und verworfen: `QtConcurrent::run` je Aufruf (der Zustand bliebe im GUI-Thread und
würde vom Pool-Thread angefasst — jeder Zugriff einzeln zu sichern); Aufrufe blockierend im
GUI-Thread (einfachster Aufbau, aber ein schlafender Server friert das Fenster ein, und der
schlafende Server ist laut Spec 10 der Normalfall, nicht die Ausnahme).

### 2.3 Ein veralteter Pin wird erklärt, nicht umgangen

Passt der gespeicherte Pin nicht mehr zum Zertifikat des Servers, führt das zu einer
eigenen Ansicht: was passiert ist, was es bedeuten kann, und genau zwei Wege — abmelden und
neu paaren, oder beenden. Kein „trotzdem fortfahren".

Begründung: Spec 2.9 verlangt den harten Abbruch und verwirft eine Rückfrage im laufenden
Betrieb ausdrücklich, weil sie jeden MITM durchwinken würde. Was dort fehlte, ist nicht die
Härte, sondern die Sprache: bisher bricht die Verbindung ohne Erklärung und ohne Angebot ab.
Diese Entscheidung ändert an der Regel nichts und macht sie nur bedienbar.

Erwogen und verworfen: stiller Rückfall auf die reguläre Prüfung, wenn das neue Zertifikat
gegen den System-Speicher besteht. Bequem, aber es gibt genau den Schutz auf, um den es beim
Anheften ging — wer ein gültiges Zertifikat für den Namen erlangt, würde angenommen.
Ebenfalls verworfen: beide Fingerabdrücke nebeneinander zeigen und bestätigen lassen; das
ist die Rückfrage, die Spec 2.9 benennt und ablehnt.

### 2.4 Die Erneuerung wird als wiederverwendbare Klasse in `core/` gebaut

`AuthenticatedClient` umschließt `HttpClient`, `Session` und `SessionRepository`. Jeder
Aufruf geht hindurch; ein 401 löst **genau eine** Erneuerung samt einer Wiederholung aus.

Begründung: Stufe 2 schickt jeden Sync-Aufruf über denselben Pfad. Läge die Regel in der
Bridge, läge sie in Qt-Code — nicht mit GTest prüfbar, und Stufe 2 baut sie nach. In
`core/` ist sie gegen `FakeTransport` vollständig prüfbar, einschließlich der Frage, die
sonst niemand stellt: dass nach einer abgelehnten Erneuerung **nicht** noch einmal versucht
wird.

Erwogen und verworfen: nur `refreshAccessToken()` in `core/` und die Wiederholung in der
Bridge; die Erneuerung ganz auf Stufe 2 verschieben (Zugangstoken leben Minuten bis Stunden
— Neupaaren wäre damit Alltag statt Ausnahme).

### 2.5 Die Gerätekennung wird einmal erzeugt und überlebt das Abmelden

Beim ersten Start eine `QUuid`, abgelegt in den Einstellungen — **nicht** in der Sitzung.
`device_name` ist der Rechnername.

Begründung: Der Server führt aus `device_id` seine Geräteliste in der Web-Oberfläche. Läge
die Kennung in der Sitzung, bekäme jedes Neupaaren einen neuen Eintrag, und das Aufräumen
wäre Handarbeit im Browser. Die Kennung gehört der Installation, nicht der Bindung an einen
Server.

Erwogen und verworfen: `QSysInfo::machineUniqueId()` — nichts zu speichern, aber
maschinen- statt installationsgebunden, und auf manchen Systemen leer. Ebenfalls verworfen:
je Paarung neu erzeugen.

### 2.6 Erneuern schreibt nur das Geheimnis, nicht die Datei

`SessionRepository` bekommt `bool saveAccessToken(const std::string&)` neben `save()`.

Begründung: Beim Erneuern ändert sich ausschließlich das Zugangstoken. Adresse,
Benutzername und Pin — der gesamte Inhalt der JSON-Datei — bleiben unberührt. Die Übergabe
führt als offenen Punkt, dass `save()` erst die Geheimnisse ablegt und dann die Datei
schreibt, und dass ein harter Absturz dazwischen neue Token neben alte Metadaten stellt;
sie merkt an, dass die Erneuerung diesen Pfad künftig ständig fährt. Ein enger Weg, der die
Datei gar nicht anfasst, nimmt dem heißen Pfad dieses Zeitfenster vollständig, statt es zu
verkleinern.

### 2.7 Ein schlafender Server behält die verbundene Ansicht

Nachgetragen am 2026-07-28, nach dem Abschlussreview der Verdrahtung.

Der Entwurf beschrieb den Ablauf in Abschnitt 4.1 richtig, gab dem Signal aber
zu wenig mit: `serverUnreachable` trug nur eine Meldung, und das Fenster kannte
an dieser Stelle weder Adresse noch Benutzernamen. Beim Start steht der
Ansichtsstapel auf der Serveradresse-Seite — der Nutzer landete also mit einer
Fehlermeldung dort und sah aus, als sei er gar nicht gepaart.

Das Signal trägt die Sitzungsangaben deshalb mit. Ein nicht erreichbarer Server
ist laut Abschnitt 10 der übergeordneten Spec ein normaler Betriebszustand; die
Sitzung bleibt bestehen, und mit ihr die verbundene Ansicht — nur mit einem
Hinweis darüber.

---

## 3. Architektur

```
src/app/                     GUI-Thread, nur Qt
├── MainWindow               QStackedWidget; besitzt QThread und AuthBridge
├── pairing/
│   ├── ServerUrlPage            Adresse eingeben
│   ├── CertificatePage          Fingerabdruck zeigen und bestätigen
│   ├── PairingCodePage          Code, Restlaufzeit, Abbrechen
│   └── CertificateChangedPage   Pin passt nicht mehr
└── ConnectedPage            „verbunden mit … als …" und Abmelden

src/bridge/                  Auth-Thread — die einzige Thread-Grenze
└── AuthBridge               QObject; besitzt HttpClient, TrustNegotiation,
                             PairingClient, AuthenticatedClient, Repository,
                             SecretStore und den Poll-Zeitgeber

src/core/                    Qt-frei, mit GTest geprüft
├── api/TrustNegotiation     NEU — die Vertrauensentscheidung als Zustandsobjekt
└── auth/AuthenticatedClient NEU — 401 → erneuern → genau einmal wiederholen
```

### 3.1 `core/api/TrustNegotiation`

Zieht den Ablauf „nicht vertrauenswürdig → Fingerabdruck zeigen → bestätigen → anheften →
erneut prüfen" aus `tools/probe/main.cpp` heraus, wo er mit `std::cin`/`std::cout`
verwachsen ist. Die Übergabe benennt das als Falle: der `PairingClient` bekam die
ein-/ausgabefreie Behandlung genau deshalb, dieser Ablauf nicht — wer den Dialog schreibt,
baut ihn sonst von Null nach, ohne Tests zu erben.

```cpp
enum class TrustState {
    Idle,                  // noch kein Kontakt
    Trusted,               // regulär geprüft oder Pin passt
    ConfirmationRequired,  // unbekanntes Zertifikat, prompt() belegt
    PinMismatch,           // Spec 2.9: harter Abbruch
    Unreachable,           // Netz, Zeitlimit — später erneut
    Failed                 // Zertifikat unlesbar, unerwarteter Status
};

struct TrustPrompt {
    std::string fingerprintSha256;  // zum Anzeigen
    std::string publicKeyPin;       // zum Anheften
    std::string reason;             // warum die Prüfung fehlschlug
};

class TrustNegotiation {
public:
    explicit TrustNegotiation(HttpClient& http);

    TrustState probe();    // erster Kontakt mit der aktuellen Regel
    TrustState confirm();  // Pin anheften und erneut prüfen

    const std::optional<TrustPrompt>& prompt() const;
    const std::string& confirmedPin() const;  // leer bei regulärer Prüfung
    TrustState state() const;
    const std::string& message() const;
};
```

`confirm()` ist nur nach `ConfirmationRequired` zulässig und bleibt sonst wirkungslos —
dieselbe Bauart, mit der `PairingClient::poll()` nach einem Endzustand nicht mehr ins Netz
greift.

### 3.2 `core/auth/AuthenticatedClient`

```cpp
enum class AuthOutcome {
    Ok,              // Antwort da (nicht notwendig 2xx), Sitzung trägt
    Refreshed,       // wie Ok, das Zugangstoken wurde unterwegs erneuert
    ReauthRequired,  // Erneuerung abgelehnt — nur Neupaaren hilft
    TrustBroken,     // Pin passt nicht — Spec 2.9
    Unreachable      // Netz, Zeitlimit — Sitzung unberührt
};

struct AuthenticatedResult {
    HttpResult response;
    AuthOutcome outcome = AuthOutcome::Ok;
};

class AuthenticatedClient {
public:
    AuthenticatedClient(HttpClient& http, SessionRepository& repository, Session session);

    AuthenticatedResult get(const std::string& path);
    AuthenticatedResult postJson(const std::string& path, const std::string& jsonBody);

    const Session& session() const;
};
```

Zwei Eigenschaften tragen die Klasse:

**Genau ein Erneuerungsversuch je Aufruf.** Beantwortet der Server auch die Wiederholung
mit 401, ist das Ergebnis `Ok` mit Status 401 — nicht eine zweite Erneuerung. Ohne diese
Grenze dreht sich der Client im Kreis.

**`ReauthRequired` ist von `Unreachable` unterschieden.** Die Übergabe benennt das
ausdrücklich: ein abgelaufenes Erneuerungstoken verlangt Neupaaren, ein schlafender Server
verlangt Geduld. Beides sieht an der Aufrufstelle gleich aus, wenn man es nicht trennt.

Das Erneuerungstoken wird vom Server **nicht rotiert** — erneuert wird ausschließlich das
Zugangstoken, abgelegt über `saveAccessToken()`.

---

## 4. Datenfluss

### 4.1 Start

```
MainWindow -> AuthBridge::restoreSession()            [Auth-Thread]
  Session laden
   ├─ keine       -> noSession()                          -> ServerUrlPage
   └─ vorhanden   -> AuthenticatedClient.get("/api/auth/me")
        ├─ Ok / Refreshed -> sessionEstablished(url, user) -> ConnectedPage
        ├─ ReauthRequired -> reauthRequired(text)          -> ServerUrlPage + Hinweis
        ├─ TrustBroken    -> certificateChanged(url)       -> CertificateChangedPage
        └─ Unreachable    -> serverUnreachable(text)       -> ConnectedPage, Zustand „offline"
```

Ein nicht erreichbarer Server ist laut Spec Abschnitt 10 ein normaler Betriebszustand. Die
Sitzung bleibt dabei bestehen; sie wird ausschließlich bei `ReauthRequired` als hinfällig
behandelt.

### 4.2 Paaren

```
ServerUrlPage --connectToServer(url)--> checkServerUrl()        [core, nicht im Dialog]
   ├─ Problem -> serverUrlRejected(msg)                         (Eingabe bleibt stehen)
   └─ ok      -> TrustNegotiation::probe()
        ├─ Trusted              -> weiter
        ├─ ConfirmationRequired -> certificateConfirmationRequired(abdruck, grund)
        │                          -> CertificatePage
        │                          --confirmCertificate()--> confirm()
        │                               ├─ Trusted -> weiter
        │                               └─ sonst   -> pairingFailed(msg)
        ├─ PinMismatch          -> certificateChanged(url)
        └─ Unreachable / Failed -> pairingFailed(msg)

weiter: PairingClient::begin(deviceId, deviceName, now)
          -> pairingCodeReady(code, verificationUrl, expiresIn) -> PairingCodePage
        QTimer, single-shot, progress.waitSeconds               [Auth-Thread]
          -> poll(now)
               ├─ WaitingForApproval -> Zeitgeber neu stellen
               ├─ Approved           -> repository.save()  SOFORT
               │                        -> sessionEstablished    -> ConnectedPage
               └─ Denied/Expired/Failed -> pairingFailed(msg)    -> ServerUrlPage
```

Der Poll-Zeitgeber lebt im Auth-Thread. Die Restlaufzeit-Anzeige („noch 9:41") ist ein
getrennter 1-Sekunden-Zeitgeber im GUI-Thread und fasst nichts an, was blockiert.

Zwischen `PairingState::Approved` und `repository.save()` steht nichts, was scheitern kann
— der Server löscht seinen Datensatz beim Ausliefern und antwortet danach mit 404.

**Zwei Abbruchwege.** `rejectCertificate()` auf der CertificatePage verwirft die
Verhandlung, ohne etwas anzuheften, und führt zurück auf die ServerUrlPage — die eingegebene
Adresse bleibt stehen. `cancelPairing()` auf der PairingCodePage hält den Zeitgeber an und
führt ebenfalls dorthin zurück; der Gerätecode verfällt beim Server von selbst, es ist
nichts zurückzunehmen. Beide melden dasselbe Signal `pairingCancelled()`, damit die
Oberfläche einen und nicht zwei Rückwege kennt.

### 4.3 Abmelden

`ConnectedPage --logout()--> repository.clear()`. Der Rückgabewert wird ausgewertet:
bleibt ein Token zurück, sagt die Oberfläche das, statt „erledigt" zu melden. Danach
`loggedOut()` → `ServerUrlPage`.

### 4.4 Signale und Slots der `AuthBridge`

**Slots** (aus dem GUI-Thread aufgerufen, im Auth-Thread ausgeführt):
`restoreSession()`, `connectToServer(QString)`, `confirmCertificate()`,
`rejectCertificate()`, `cancelPairing()`, `logout()`

**Signale** (im GUI-Thread zugestellt, `Qt::QueuedConnection`):
`noSession()`, `sessionEstablished(QString serverUrl, QString username)`,
`serverUnreachable(QString serverUrl, QString username, QString message)`,
`serverUrlRejected(QString)`,
`certificateConfirmationRequired(QString fingerprint, QString reason)`,
`certificateChanged(QString serverUrl)`, `pairingCodeReady(QString code, QString url, int expiresIn)`,
`pairingNotice(QString)`, `pairingFailed(QString)`, `pairingCancelled()`,
`reauthRequired(QString)`, `loggedOut()`, `logoutIncomplete(QString)`

---

## 5. Fehlerbehandlung

| Fall | Verhalten | Sichtbarkeit |
|---|---|---|
| Adresse ungültig, `http://` zu fremdem Rechner | `checkServerUrl` lehnt **vor** jedem Netzkontakt ab | ServerUrlPage, Meldung unter dem Feld |
| Zertifikat unbekannt | Fingerabdruck zeigen, einmalig bestätigen | CertificatePage |
| Pin passt nicht | harter Abbruch, kein „trotzdem" | CertificateChangedPage |
| Server beim Paaren nicht erreichbar | Meldung, Eingabe bleibt stehen | ServerUrlPage |
| Netzfehler während des Abrufens | `PairingClient` wartet weiter, solange der Code lebt | PairingCodePage, unverändert |
| Ratenbegrenzung (429) beim Abrufen | Wartezeit aus dem Rumpf einhalten | PairingCodePage, Hinweis |
| Code abgelaufen oder abgelehnt | Vorgang beendet | ServerUrlPage + Meldung |
| `save()` scheitert nach der Freigabe | ausdrücklich als Verlust melden — die Token gibt es kein zweites Mal | ServerUrlPage, deutliche Meldung |
| Zugangstoken abgelaufen | still erneuern, Aufruf einmal wiederholen | unsichtbar |
| Erneuerungstoken abgelaufen | `ReauthRequired` | ServerUrlPage + „Sitzung abgelaufen" |
| Server schläft beim Start | Sitzung bleibt, Zustand „offline" | ConnectedPage |
| `clear()` nur teilweise erfolgreich | benennen statt verschweigen | ConnectedPage bzw. ServerUrlPage |

---

## 6. Teststrategie

**Neu in `tests/core/`**

`trust_negotiation_test.cpp` gegen `FakeTransport`: alle sechs Zustände; `confirm()` ohne
vorheriges `ConfirmationRequired` fasst das Netz nicht an; ein bestätigtes Zertifikat, das
danach trotzdem nicht trägt; unlesbares Zertifikat trotz `CertificateUntrusted`.

`authenticated_client_test.cpp` gegen `FakeTransport` und einen `FakeSessionRepository`:
2xx ohne Erneuerung; 401 → Erneuerung → Wiederholung → `Refreshed`; das erneuerte Token
wird abgelegt; 401 auch auf die Wiederholung ergibt `Ok` mit Status 401 und **keine**
zweite Erneuerung; 401 auf `/api/auth/refresh` ergibt `ReauthRequired`; Netzfehler beim
Erneuern ergibt `Unreachable` und lässt die Sitzung unberührt;
`CertificatePinMismatch` ergibt `TrustBroken` ohne Erneuerungsversuch.

**Erweitert in `tests/app/`**

Die Gerätekennung: einmal erzeugt, über Neustarts stabil, überlebt das Abmelden.

**Bewusste Lücke**

Keine automatisierten Oberflächentests (Spec Abschnitt 4 und 11.3). Stattdessen eine
Abnahmeliste gegen den laufenden Server, wie in den Stufen 1a und 1b.

---

## 7. Offene Punkte aus der Übergabe, die hier mitlaufen

Alle liegen in Dateien, die diese Stufe ohnehin anfasst:

- `SessionRepository::clear()` gibt `bool` zurück statt `void` — der Abmelde-Knopf kann
  sonst nur „erledigt" sagen, auch wenn ein Token im Speicher zurückblieb
- `defaultSessionPath()` weicht nicht mehr auf `"."` aus, sondern hält die Zusicherung
  „immer absolut", die der eigene Header gibt
- `std::error_code` von `create_directories`, `remove` und `rename` wird ausgewertet
- `tools/probe` fährt auf `TrustNegotiation` statt auf seinem eingebauten Ablauf — damit
  ist die Extraktion belegt und nicht bloß behauptet

Nicht in dieser Stufe: das fehlende `compile_commands.json`, die Version an drei Stellen,
`windeployqt` ohne MSVC-Laufzeitbibliotheken (Stufe 7), die stille Vorgabe bei
`"interval": 5.0`.

---

## 8. Nicht-Ziele der Stufe 1c

Synchronisation, Datenbank, Infobereichssymbol, Autostart, Benachrichtigungen, Dashboard,
Explorer, Einstellungsdialog, Ändern des Gerätenamens, mehrere Server-Profile.

---

## 9. Abnahmekriterium

Das aus Abschnitt 12 der übergeordneten Spec, Stufe 1 — diesmal über die Oberfläche statt
über das Prüfprogramm:

**Ein Neustart führt ohne erneutes Pairing zu einer authentifizierten Sitzung.**

Dazu, gegen den laufenden Server zu belegen:

| Prüfung | Erwartung |
|---|---|
| `http://` zu einem fremden Rechner eingeben | abgelehnt vor jedem Netzkontakt, Meldung am Feld |
| Adresse mit gültigem Zertifikat eingeben | Code erscheint ohne Zertifikatsnachfrage |
| Freigabe im Browser | Ansicht wechselt selbsttätig auf „verbunden mit … als …" |
| Anwendung beenden und neu starten | sofort verbunden, ohne Paaren |
| Adresse mit unbekanntem Zertifikat eingeben | Fingerabdruck erscheint, nach Bestätigung wird gepaart |
| Pin in `session.json` verfälschen, dann starten | CertificateChangedPage, nicht stiller Abbruch |
| „Abmelden & neu" auf dieser Seite | Sitzung entfernt, Paarungsansicht |
| Abmelden aus der verbundenen Ansicht | Datei und beide Token entfernt, Paarungsansicht |
| Während der Codeanzeige das Fenster verschieben | Fenster bleibt bedienbar (belegt den Arbeiterthread) |

Die letzte Zeile ist der Nachweis für Entscheidung 2.2: sie ist der einzige Weg, das
Nicht-Einfrieren zu zeigen, und ohne sie bliebe der Arbeiterthread eine Behauptung.
