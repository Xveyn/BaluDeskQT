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
    // hadPinnedKey entscheidet über den Text, nicht über die Härte: mit
    // angeheftetem Schlüssel weist der Server ein anderes Zertifikat vor als
    // das bestätigte, ohne ihn ist nie eines bestätigt worden und die
    // reguläre Prüfung schlägt fehl. Beides endet hier, aber die falsche der
    // beiden Diagnosen schickt den Nutzer auf die Suche nach einem Angreifer,
    // wo bloss ein Zertifikat abgelaufen ist.
    void showChange(const QString& serverUrl, const QString& reason, bool hadPinnedKey);

signals:
    // Abmelden und neu paaren - der einzige Weg zurück.
    void repairRequested();
    void quitRequested();

private:
    QLabel* heading_ = nullptr;
    QLabel* explanation_ = nullptr;
    QLabel* reason_ = nullptr;
    QLabel* consequence_ = nullptr;
};

}  // namespace baludesk::app
