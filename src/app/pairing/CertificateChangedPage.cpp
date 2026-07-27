#include "app/pairing/CertificateChangedPage.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace baludesk::app {

CertificateChangedPage::CertificateChangedPage(QWidget* parent) : QWidget(parent) {
    auto* heading = new QLabel(tr("⚠  Das Zertifikat hat sich geändert"), this);
    QFont headingFont = heading->font();
    headingFont.setPointSize(headingFont.pointSize() + 6);
    heading->setFont(headingFont);

    explanation_ = new QLabel(this);
    explanation_->setWordWrap(true);

    reason_ = new QLabel(this);
    reason_->setWordWrap(true);

    auto* consequence = new QLabel(
        tr("Das ist normal, wenn der Server ein neues Zertifikat bekommen hat. Es kann "
           "aber auch bedeuten, dass sich jemand dazwischenschaltet.\n\n"
           "Die Verbindung bleibt getrennt, bis du dich neu verbindest. Vergewissere "
           "dich vorher, dass der Server tatsächlich ein neues Zertifikat hat."),
        this);
    consequence->setWordWrap(true);

    auto* quit = new QPushButton(tr("Beenden"), this);
    auto* repair = new QPushButton(tr("Abmelden und neu verbinden"), this);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    buttons->addWidget(quit);
    buttons->addWidget(repair);

    auto* layout = new QVBoxLayout(this);
    layout->addStretch(1);
    layout->addWidget(heading);
    layout->addSpacing(12);
    layout->addWidget(explanation_);
    layout->addSpacing(8);
    layout->addWidget(reason_);
    layout->addSpacing(8);
    layout->addWidget(consequence);
    layout->addSpacing(16);
    layout->addLayout(buttons);
    layout->addStretch(2);

    connect(repair, &QPushButton::clicked, this, &CertificateChangedPage::repairRequested);
    connect(quit, &QPushButton::clicked, this, &CertificateChangedPage::quitRequested);
}

void CertificateChangedPage::showChange(const QString& serverUrl, const QString& reason) {
    explanation_->setText(
        tr("Der Server %1 weist ein anderes Zertifikat vor als das, das beim Verbinden "
           "bestätigt wurde.")
            .arg(serverUrl));
    reason_->setText(reason.isEmpty() ? QString() : tr("Meldung: %1").arg(reason));
    reason_->setVisible(!reason.isEmpty());
}

}  // namespace baludesk::app
