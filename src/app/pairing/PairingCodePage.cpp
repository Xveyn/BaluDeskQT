#include "app/pairing/PairingCodePage.h"

#include <QDesktopServices>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include "app/pairing/Countdown.h"

namespace baludesk::app {

PairingCodePage::PairingCodePage(QWidget* parent) : QWidget(parent) {
    auto* heading = new QLabel(tr("Code im Browser eingeben"), this);
    QFont headingFont = heading->font();
    headingFont.setPointSize(headingFont.pointSize() + 6);
    heading->setFont(headingFont);

    code_ = new QLabel(this);
    QFont codeFont = code_->font();
    codeFont.setPointSize(codeFont.pointSize() + 22);
    codeFont.setBold(true);
    // Weit gesperrt, damit sich die sechs Ziffern einzeln ablesen lassen.
    codeFont.setLetterSpacing(QFont::AbsoluteSpacing, 8);
    code_->setFont(codeFont);
    code_->setAlignment(Qt::AlignCenter);
    code_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    url_ = new QLabel(this);
    url_->setWordWrap(true);
    url_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* openBrowser = new QPushButton(tr("Im Browser öffnen"), this);

    remaining_ = new QLabel(this);
    notice_ = new QLabel(this);
    notice_->setWordWrap(true);
    notice_->setVisible(false);

    auto* cancel = new QPushButton(tr("Abbrechen"), this);

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(openBrowser);
    buttons->addStretch(1);
    buttons->addWidget(cancel);

    auto* layout = new QVBoxLayout(this);
    layout->addStretch(1);
    layout->addWidget(heading);
    layout->addSpacing(16);
    layout->addWidget(code_);
    layout->addSpacing(16);
    layout->addWidget(url_);
    layout->addWidget(remaining_);
    layout->addWidget(notice_);
    layout->addSpacing(16);
    layout->addLayout(buttons);
    layout->addStretch(2);

    tick_ = new QTimer(this);
    tick_->setInterval(1000);
    connect(tick_, &QTimer::timeout, this, &PairingCodePage::updateRemaining);

    connect(cancel, &QPushButton::clicked, this, [this]() {
        stop();
        emit cancelled();
    });
    connect(openBrowser, &QPushButton::clicked, this, [this]() {
        if (!verificationUrl_.isEmpty()) {
            QDesktopServices::openUrl(QUrl(verificationUrl_));
        }
    });
}

void PairingCodePage::showCode(const QString& userCode,
                               const QString& verificationUrl,
                               int expiresInSeconds) {
    code_->setText(userCode);
    verificationUrl_ = verificationUrl;
    url_->setText(tr("Öffne %1, melde dich an und gib den Code ein.").arg(verificationUrl));
    notice_->clear();
    notice_->setVisible(false);

    remainingSeconds_ = expiresInSeconds;
    updateRemaining();
    tick_->start();
}

void PairingCodePage::showNotice(const QString& message) {
    notice_->setText(message);
    notice_->setVisible(!message.isEmpty());
}

void PairingCodePage::stop() {
    tick_->stop();
}

void PairingCodePage::updateRemaining() {
    remaining_->setText(tr("Warte auf Freigabe…  (noch %1)").arg(formatRemaining(remainingSeconds_)));
    if (remainingSeconds_ <= 0) {
        // Die Bridge meldet das Ablaufen selbst; hier bleibt die Anzeige
        // einfach stehen, statt ins Negative zu laufen.
        tick_->stop();
        return;
    }
    --remainingSeconds_;
}

}  // namespace baludesk::app
