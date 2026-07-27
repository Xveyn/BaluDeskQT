#pragma once

#include <QWidget>

class QLabel;

namespace baludesk::app {

// Sackgasse mit genau zwei Auswegen.
//
// Spec Abschnitt 2.9 verlangt bei einer Zertifikatsabweichung den harten
// Abbruch und verwirft eine Rückfrage „trotzdem fortfahren?" ausdrücklich -
// sie würde jeden MITM durchwinken. Diese Seite nimmt der Regel nichts von
// ihrer Härte, sondern nur ihre Sprachlosigkeit.
class CertificateChangedPage : public QWidget {
    Q_OBJECT

public:
    explicit CertificateChangedPage(QWidget* parent = nullptr);

public slots:
    void showChange(const QString& serverUrl, const QString& reason);

signals:
    // Abmelden und neu paaren - der einzige Weg zurück.
    void repairRequested();
    void quitRequested();

private:
    QLabel* explanation_ = nullptr;
    QLabel* reason_ = nullptr;
};

}  // namespace baludesk::app
