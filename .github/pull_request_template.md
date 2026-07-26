## Worum geht es

<!-- Was ändert sich, und warum. Das Was steht im Diff — hier zählt das Warum. -->

## Betroffene Schicht

<!-- Zutreffendes ankreuzen -->

- [ ] `core/` — reines C++17, kennt Qt nicht
- [ ] `app/` — Qt Widgets
- [ ] `tools/` — Prüfprogramm
- [ ] Bausystem oder CI
- [ ] Nur Dokumentation

## Tests

<!-- Ausgabe von `ctest --preset debug`, mit Zahl. -->

```
```

- [ ] Neuer Code kommt mit Tests
- [ ] Wo eine Sicherung eingebaut wurde: belegt, dass der Test rot wird, wenn man sie entfernt
- [ ] Kein Test fasst echte Betriebsmittel an (Anmeldeinformationsspeicher, Netz)

## Konventionen

- [ ] `baludesk_core` linkt kein Qt, nichts unter `src/core/` bindet einen Qt-Header ein
- [ ] Bezeichner englisch, Kommentare und Protokolltexte deutsch
- [ ] Echte UTF-8-Umlaute, keine Behelfsschreibung, kein BOM — Commit-Nachricht ASCII
- [ ] Includes vollständig ab `src/`, kein `../`
- [ ] Build frei von Warnungen aus eigenem Code

## Entscheidungen

- [ ] Berührt keine Entscheidung aus der Spezifikation
- [ ] Berührt eine — die Anpassung der Spezifikation ist Teil dieses Pull Requests

## Nebenbefunde

<!-- Ist dir etwas aufgefallen, das nicht hierher gehört? Bitte hier benennen statt
     heimlich mitfixen. Ein Issue dafür ist auch recht. -->
