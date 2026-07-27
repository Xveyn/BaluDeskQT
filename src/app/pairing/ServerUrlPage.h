#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

namespace baludesk::app {

// Erste Seite: die Serveradresse eingeben.
//
// Prüft nichts selbst. Die Regel, welche Adresse zulässig ist, liegt in
// core::checkServerUrl und gilt für jeden Aufrufer - hier nachgebaut wäre sie
// eine zweite Wahrheit.
class ServerUrlPage : public QWidget {
    Q_OBJECT

public:
    explicit ServerUrlPage(QWidget* parent = nullptr);

public slots:
    // Meldung unter dem Eingabefeld, rot. Die Eingabe bleibt stehen.
    void showProblem(const QString& message);

    // Meldung unter dem Eingabefeld, unauffällig - etwa „Sitzung abgelaufen".
    void showNotice(const QString& message);

    void clearMessages();

    // Während ein Aufruf läuft. Das Feld bleibt lesbar, der Knopf wird
    // gesperrt - ein zweiter Klick würde einen zweiten Vorgang starten.
    void setBusy(bool busy);

signals:
    void connectRequested(const QString& rawUrl);

private:
    void requestConnect();

    QLineEdit* input_ = nullptr;
    QPushButton* connectButton_ = nullptr;
    QLabel* message_ = nullptr;
};

}  // namespace baludesk::app
