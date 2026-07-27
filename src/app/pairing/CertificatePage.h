#pragma once

#include <QWidget>

class QLabel;

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
    void showCertificate(const QString& fingerprint, const QString& reason);

signals:
    void confirmed();
    void rejected();

private:
    QLabel* reason_ = nullptr;
    QLabel* fingerprint_ = nullptr;
};

}  // namespace baludesk::app
