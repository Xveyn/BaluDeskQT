#include "app/ConnectedPage.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace baludesk::app {

ConnectedPage::ConnectedPage(QWidget* parent) : QWidget(parent) {
    connection_ = new QLabel(this);
    QFont connectionFont = connection_->font();
    connectionFont.setPointSize(connectionFont.pointSize() + 4);
    connection_->setFont(connectionFont);
    connection_->setWordWrap(true);

    offline_ = new QLabel(this);
    offline_->setWordWrap(true);
    offline_->setVisible(false);

    auto* outlook = new QLabel(
        tr("Die Synchronisation folgt in der nächsten Ausbaustufe."), this);
    outlook->setWordWrap(true);

    auto* logout = new QPushButton(tr("Abmelden"), this);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    buttons->addWidget(logout);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(connection_);
    layout->addWidget(offline_);
    layout->addSpacing(16);
    layout->addWidget(outlook);
    layout->addStretch(1);
    layout->addLayout(buttons);

    connect(logout, &QPushButton::clicked, this, &ConnectedPage::logoutRequested);
}

void ConnectedPage::showConnection(const QString& serverUrl, const QString& username) {
    connection_->setText(tr("Verbunden mit %1 als %2").arg(serverUrl, username));
}

void ConnectedPage::showOffline(const QString& message) {
    offline_->setText(message);
    offline_->setVisible(true);
}

void ConnectedPage::clearOffline() {
    offline_->clear();
    offline_->setVisible(false);
}

}  // namespace baludesk::app
