#pragma once

#include <QWidget>

namespace baludesk::app {

// Der Zustand vor der ersten Antwort der Bridge.
//
// Diese Seite hat keinen Knopf und kein Eingabefeld, und das ist ihr ganzer
// Zweck: restoreSession() läuft im Arbeiterthread und braucht bei einem
// schlafenden Server bis zu 30 Sekunden. Wer in dieser Zeit die Eingabeseite
// sieht, hält sich für ungepaart - genau die Aufforderung zum Neupaaren, die
// Spec Abschnitt 10 verbietet -, tippt eine Adresse und startet damit eine
// zweite Paarung, die die intakte Sitzung überschreibt.
class StartupPage : public QWidget {
    Q_OBJECT

public:
    explicit StartupPage(QWidget* parent = nullptr);
};

}  // namespace baludesk::app
