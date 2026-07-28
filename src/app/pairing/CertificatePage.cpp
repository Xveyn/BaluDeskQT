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

    reject_ = new QPushButton(tr("Abbrechen"), this);
    confirm_ = new QPushButton(tr("Vertrauen und fortfahren"), this);
    confirm_->setDefault(true);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    buttons->addWidget(reject_);
    buttons->addWidget(confirm_);

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

    connect(confirm_, &QPushButton::clicked, this, &CertificatePage::requestConfirm);
    connect(reject_, &QPushButton::clicked, this, &CertificatePage::requestReject);
}

void CertificatePage::requestConfirm() {
    // Sperren, bevor das Signal die Warteschlange des Arbeiterthreads
    // erreicht: dort blockiert confirmCertificate() bis zu 30 Sekunden, und
    // ein zweiter Klick liefe in derselben Warteschlange hinterher. Er
    // erzeugte einen zweiten Gerätecode und verbrauchte die Grenze von drei
    // Anfragen je Minute - also genau das, was den Nutzer aussperrt.
    setBusy(true);
    emit confirmed();
}

void CertificatePage::requestReject() {
    // Auch der Abbruch sperrt beide Knöpfe. Eine Entscheidung ist gefallen;
    // die zweite käme in einem Vorgang an, den es nicht mehr gibt.
    setBusy(true);
    emit rejected();
}

void CertificatePage::showCertificate(const QString& fingerprint, const QString& reason) {
    // Die Seite gehört sich selbst: sie wird nur über diesen Weg gezeigt,
    // also entsperrt sie hier auch selbst, statt sich auf den Aufrufer zu
    // verlassen.
    setBusy(false);
    reason_->setText(tr("Grund: %1").arg(reason));
    fingerprint_->setText(fingerprint);
}

void CertificatePage::setBusy(bool busy) {
    confirm_->setEnabled(!busy);
    confirm_->setText(busy ? tr("Wird geprüft…") : tr("Vertrauen und fortfahren"));
    reject_->setEnabled(!busy);
}

}  // namespace baludesk::app
