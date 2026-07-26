#include "app/QtLogBridge.h"

#include <QByteArray>
#include <QMessageLogContext>
#include <QString>
#include <QtGlobal>

#include "core/util/Logger.h"

namespace baludesk::app {
namespace {

void forwardToLogger(QtMsgType type, const QMessageLogContext& context, const QString& message) {
    const QByteArray utf8 = message.toUtf8();
    const std::string text(utf8.constData(), static_cast<std::size_t>(utf8.size()));

    // Herkunft anhängen, wenn Qt sie mitliefert. In Release-Builds fehlt sie
    // meist, deshalb nicht darauf verlassen.
    std::string origin;
    if (context.file != nullptr && context.line > 0) {
        origin = std::string(" (") + context.file + ":" + std::to_string(context.line) + ")";
    }

    switch (type) {
        case QtDebugMsg:
            core::Logger::debug("Qt: {}{}", text, origin);
            break;
        case QtInfoMsg:
            core::Logger::info("Qt: {}{}", text, origin);
            break;
        case QtWarningMsg:
            core::Logger::warn("Qt: {}{}", text, origin);
            break;
        case QtCriticalMsg:
            core::Logger::error("Qt: {}{}", text, origin);
            break;
        case QtFatalMsg:
            core::Logger::error("Qt (fatal): {}{}", text, origin);
            core::Logger::flush();
            break;
    }
}

}  // namespace

void installQtLogBridge() {
    qInstallMessageHandler(forwardToLogger);
}

}  // namespace baludesk::app
