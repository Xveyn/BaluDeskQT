#include <QApplication>
#include <QDir>
#include <QStandardPaths>

#include "app/MainWindow.h"
#include "app/SettingsStore.h"
#include "core/util/AppConfig.h"
#include "core/util/Logger.h"

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);

    // Muss vor dem ersten QSettings-Zugriff stehen, weil QSettings den
    // Speicherort daraus ableitet.
    QCoreApplication::setOrganizationName(QStringLiteral("BaluHost"));
    QCoreApplication::setApplicationName(QStringLiteral("BaluDesk"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    const baludesk::app::SettingsStore settingsStore;
    const baludesk::core::AppConfig config = settingsStore.load();

    const QString logDirectory =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/logs");
    // Rückgabewert bewusst festgehalten, aber ohne eigene Fehlerbehandlung
    // in dieser Stufe: Logger::init() fängt ein nicht anlegbares Verzeichnis
    // selbst sicher ab (siehe Logger.cpp). [[maybe_unused]], weil hier noch
    // keine Fehler-UI folgt - der Punkt ist nur, nicht länger so zu tun, als
    // existiere das Verzeichnis, wenn mkpath() das Gegenteil meldet.
    [[maybe_unused]] const bool logDirectoryReady = QDir().mkpath(logDirectory);

    baludesk::core::Logger::init(logDirectory.toStdString(),
                                 config.logMaxFileBytes,
                                 config.logMaxFiles);
    baludesk::core::Logger::info("BaluDesk {} gestartet",
                                 QCoreApplication::applicationVersion().toStdString());
    baludesk::core::Logger::info("Abfrageintervall: {} s", config.pollIntervalSeconds);

    baludesk::app::MainWindow window;
    window.show();

    const int exitCode = application.exec();

    baludesk::core::Logger::info("BaluDesk beendet, Rückgabewert {}", exitCode);
    baludesk::core::Logger::shutdown();

    return exitCode;
}
