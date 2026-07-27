#include "app/pairing/ServerUrlPage.h"

#include <QFont>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace baludesk::app {

ServerUrlPage::ServerUrlPage(QWidget* parent) : QWidget(parent) {
    auto* heading = new QLabel(tr("Mit einem Server verbinden"), this);
    QFont headingFont = heading->font();
    headingFont.setPointSize(headingFont.pointSize() + 6);
    heading->setFont(headingFont);

    auto* label = new QLabel(tr("Serveradresse"), this);

    input_ = new QLineEdit(this);
    input_->setPlaceholderText(QStringLiteral("nas.example.com"));
    input_->setClearButtonEnabled(true);

    message_ = new QLabel(this);
    message_->setWordWrap(true);
    message_->setVisible(false);

    connectButton_ = new QPushButton(tr("Verbinden"), this);
    connectButton_->setDefault(true);

    auto* layout = new QVBoxLayout(this);
    layout->addStretch(1);
    layout->addWidget(heading);
    layout->addSpacing(12);
    layout->addWidget(label);
    layout->addWidget(input_);
    layout->addWidget(message_);
    layout->addSpacing(8);
    layout->addWidget(connectButton_, 0, Qt::AlignRight);
    layout->addStretch(2);

    connect(connectButton_, &QPushButton::clicked, this, &ServerUrlPage::requestConnect);
    // Eingabetaste im Feld tut dasselbe wie der Knopf.
    connect(input_, &QLineEdit::returnPressed, this, &ServerUrlPage::requestConnect);
}

void ServerUrlPage::requestConnect() {
    clearMessages();
    emit connectRequested(input_->text());
}

void ServerUrlPage::showProblem(const QString& message) {
    setBusy(false);
    message_->setText(message);
    message_->setStyleSheet(QStringLiteral("color: palette(bright-text);"));
    message_->setVisible(true);
    input_->setFocus();
}

void ServerUrlPage::showNotice(const QString& message) {
    setBusy(false);
    message_->setText(message);
    message_->setStyleSheet(QString());
    message_->setVisible(true);
}

void ServerUrlPage::clearMessages() {
    message_->clear();
    message_->setVisible(false);
}

void ServerUrlPage::setBusy(bool busy) {
    connectButton_->setEnabled(!busy);
    connectButton_->setText(busy ? tr("Verbinde…") : tr("Verbinden"));
}

}  // namespace baludesk::app
