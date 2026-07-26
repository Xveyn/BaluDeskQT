#include "app/MainWindow.h"

#include <QLabel>
#include <QCoreApplication>

namespace baludesk::app {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("BaluDesk %1").arg(QCoreApplication::applicationVersion()));
    resize(900, 600);

    auto* placeholder = new QLabel(
        QStringLiteral("BaluDesk QT — Gerüst steht.\nStufe 1 bringt das Geräte-Pairing."),
        this);
    placeholder->setAlignment(Qt::AlignCenter);
    setCentralWidget(placeholder);
}

}  // namespace baludesk::app
