#include "app/pairing/CertificateChangedPage.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace baludesk::app {

CertificateChangedPage::CertificateChangedPage(QWidget* parent) : QWidget(parent) {
    heading_ = new QLabel(this);
    QFont headingFont = heading_->font();
    headingFont.setPointSize(headingFont.pointSize() + 6);
    heading_->setFont(headingFont);
    heading_->setWordWrap(true);

    explanation_ = new QLabel(this);
    explanation_->setWordWrap(true);

    reason_ = new QLabel(this);
    reason_->setWordWrap(true);

    consequence_ = new QLabel(this);
    consequence_->setWordWrap(true);

    auto* quit = new QPushButton(tr("Beenden"), this);
    auto* repair = new QPushButton(tr("Abmelden und neu verbinden"), this);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    buttons->addWidget(quit);
    buttons->addWidget(repair);

    auto* layout = new QVBoxLayout(this);
    layout->addStretch(1);
    layout->addWidget(heading_);
    layout->addSpacing(12);
    layout->addWidget(explanation_);
    layout->addSpacing(8);
    layout->addWidget(reason_);
    layout->addSpacing(8);
    layout->addWidget(consequence_);
    layout->addSpacing(16);
    layout->addLayout(buttons);
    layout->addStretch(2);

    connect(repair, &QPushButton::clicked, this, &CertificateChangedPage::repairRequested);
    connect(quit, &QPushButton::clicked, this, &CertificateChangedPage::quitRequested);
}

void CertificateChangedPage::showChange(const QString& serverUrl,
                                        const QString& reason,
                                        bool hadPinnedKey) {
    if (hadPinnedKey) {
        heading_->setText(tr("⚠  Das Zertifikat hat sich geändert"));
        explanation_->setText(
            tr("Der Server %1 weist ein anderes Zertifikat vor als das, das beim "
               "Verbinden bestätigt wurde.")
                .arg(serverUrl));
        consequence_->setText(
            tr("Das ist normal, wenn der Server ein neues Zertifikat bekommen hat. Es "
               "kann aber auch bedeuten, dass sich jemand dazwischenschaltet.\n\n"
               "Die Verbindung bleibt getrennt, bis du dich neu verbindest. "
               "Vergewissere dich vorher, dass der Server tatsächlich ein neues "
               "Zertifikat hat."));
    } else {
        // Ohne angehefteten Schlüssel wurde beim Verbinden nie ein Zertifikat
        // bestätigt: der Server hatte ein öffentlich vertrauenswürdiges. Von
        // einem Austausch zu sprechen wäre schlicht falsch, und die Warnung
        // vor einem Mithörer schickte den Nutzer auf eine Fährte, die es
        // meist nicht gibt.
        heading_->setText(tr("⚠  Dem Zertifikat wird nicht mehr vertraut"));
        explanation_->setText(
            tr("Das Zertifikat des Servers %1 lässt sich nicht mehr prüfen. Beim "
               "Verbinden war es regulär vertrauenswürdig, es wurde deshalb keines "
               "bestätigt und keines angeheftet.")
                .arg(serverUrl));
        consequence_->setText(
            tr("Meist ist das Zertifikat abgelaufen oder die ausstellende Stelle wird "
               "nicht mehr anerkannt. Ausschliessen lässt sich ein Mithörer aber "
               "nicht.\n\n"
               "Die Verbindung bleibt getrennt, bis du dich neu verbindest. "
               "Vergewissere dich vorher, dass auf dem Server tatsächlich ein neues "
               "oder erneuertes Zertifikat liegt."));
    }

    reason_->setText(reason.isEmpty() ? QString() : tr("Meldung: %1").arg(reason));
    reason_->setVisible(!reason.isEmpty());
}

}  // namespace baludesk::app
