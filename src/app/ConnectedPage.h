#pragma once

#include <QWidget>

class QLabel;

namespace baludesk::app {

// Der verbundene Zustand. In Stufe 1c zeigt sie, mit wem man verbunden ist,
// und bietet das Abmelden an; ab Stufe 2 kommt der Sync-Zustand hierher.
class ConnectedPage : public QWidget {
    Q_OBJECT

public:
    explicit ConnectedPage(QWidget* parent = nullptr);

public slots:
    void showConnection(const QString& serverUrl, const QString& username);

    // Ein schlafender Server ist laut Spec Abschnitt 10 ein normaler
    // Betriebszustand, kein Fehler - die Sitzung bleibt bestehen.
    void showOffline(const QString& message);
    void clearOffline();

signals:
    void logoutRequested();

private:
    QLabel* connection_ = nullptr;
    QLabel* offline_ = nullptr;
};

}  // namespace baludesk::app
