#pragma once

#include <QWidget>

class QLabel;
class QTimer;

namespace baludesk::app {

// Dritte Seite: den Code zeigen und auf die Freigabe warten.
//
// Der Zeitgeber hier zählt nur die Anzeige herunter und fasst nichts an, was
// blockiert. Die eigentliche Abfrage läuft in der AuthBridge, in deren
// Thread.
class PairingCodePage : public QWidget {
    Q_OBJECT

public:
    explicit PairingCodePage(QWidget* parent = nullptr);

public slots:
    void showCode(const QString& userCode, const QString& verificationUrl, int expiresInSeconds);
    void showNotice(const QString& message);

    // Hält den Countdown an, etwa beim Verlassen der Seite.
    void stop();

signals:
    void cancelled();

private:
    void updateRemaining();

    QLabel* code_ = nullptr;
    QLabel* url_ = nullptr;
    QLabel* remaining_ = nullptr;
    QLabel* notice_ = nullptr;
    QTimer* tick_ = nullptr;
    QString verificationUrl_;
    int remainingSeconds_ = 0;
};

}  // namespace baludesk::app
