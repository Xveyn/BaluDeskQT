#include "app/pairing/CertificatePage.h"

#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace baludesk::app {

CertificatePage::CertificatePage(QWidget* parent) : QWidget(parent) {
    auto* heading = new QLabel(tr("Zertifikat des Servers bestätigen"), this);
    QFont headingFont = heading->font();
    headingFont.setPointSize(headingFont.pointSize() + 6);
    heading->setFont(headingFont);

    auto* explanation = new QLabel(
        tr("Dem Zertifikat dieses Servers wird nicht automatisch vertraut. "
           "Vergleiche den Fingerabdruck mit dem, den der Server anzeigt. "
           "Stimmen beide überein, ist die Verbindung sicher."),
        this);
    explanation->setWordWrap(true);

    reason_ = new QLabel(this);
    reason_->setWordWrap(true);

    fingerprint_ = new QLabel(this);
    fingerprint_->setWordWrap(true);
    // Feste Zeichenbreite: nur so lassen sich zwei Hexfolgen sinnvoll
    // nebeneinanderhalten.
    fingerprint_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    // Zum Kopieren, damit der Vergleich nicht abgetippt werden muss.
    fingerprint_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* reject = new QPushButton(tr("Abbrechen"), this);
    auto* confirm = new QPushButton(tr("Vertrauen und fortfahren"), this);
    confirm->setDefault(true);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    buttons->addWidget(reject);
    buttons->addWidget(confirm);

    auto* layout = new QVBoxLayout(this);
    layout->addStretch(1);
    layout->addWidget(heading);
    layout->addSpacing(12);
    layout->addWidget(explanation);
    layout->addSpacing(8);
    layout->addWidget(reason_);
    layout->addSpacing(8);
    layout->addWidget(fingerprint_);
    layout->addSpacing(16);
    layout->addLayout(buttons);
    layout->addStretch(2);

    connect(confirm, &QPushButton::clicked, this, &CertificatePage::confirmed);
    connect(reject, &QPushButton::clicked, this, &CertificatePage::rejected);
}

void CertificatePage::showCertificate(const QString& fingerprint, const QString& reason) {
    reason_->setText(tr("Grund: %1").arg(reason));
    fingerprint_->setText(fingerprint);
}

}  // namespace baludesk::app
