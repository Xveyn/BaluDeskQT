#include <QApplication>

#include "app/MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral("BaluHost"));
    QCoreApplication::setApplicationName(QStringLiteral("BaluDesk"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    baludesk::app::MainWindow window;
    window.show();

    return application.exec();
}
