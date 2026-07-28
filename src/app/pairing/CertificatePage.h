#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

namespace baludesk::app {

// Zweite Seite, nur bei Bedarf: den Fingerabdruck vergleichen lassen.
//
// Der Fingerabdruck steht in derselben Darstellung, die auch Browser und die
// openssl-Kommandozeile ausgeben - sonst wäre er nicht zu vergleichen, und
// genau das Vergleichen ist der ganze Sinn dieser Seite.
class CertificatePage : public QWidget {
    Q_OBJECT

public:
    explicit CertificatePage(QWidget* parent = nullptr);

public slots:
    // Setzt die Seite zugleich wieder in den bedienbaren Zustand: hierher
    // führt jeder Weg zurück, der die Seite erneut zeigt.
    void showCertificate(const QString& fingerprint, const QString& reason);

    // Während der Aufruf hinter der Entscheidung läuft. Gesperrt werden beide
    // Knöpfe, nicht nur der gedrückte - siehe die Begründung in der
    // Quelldatei.
    void setBusy(bool busy);

signals:
    void confirmed();
    void rejected();

private:
    void requestConfirm();
    void requestReject();

    QLabel* reason_ = nullptr;
    QLabel* fingerprint_ = nullptr;
    QPushButton* confirm_ = nullptr;
    QPushButton* reject_ = nullptr;
};

}  // namespace baludesk::app
