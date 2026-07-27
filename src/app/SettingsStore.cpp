#include "app/SettingsStore.h"

#include <QSysInfo>
#include <QUuid>

namespace baludesk::app {
namespace {

constexpr const char* kPollIntervalSeconds = "sync/pollIntervalSeconds";
constexpr const char* kStartMinimized = "ui/startMinimized";
constexpr const char* kAutostartWithWindows = "ui/autostartWithWindows";
constexpr const char* kNotifyOnError = "notifications/onError";
constexpr const char* kNotifyOnSyncComplete = "notifications/onSyncComplete";
constexpr const char* kLogMaxFileBytes = "logging/maxFileBytes";
constexpr const char* kLogMaxFiles = "logging/maxFiles";
constexpr const char* kDeviceId = "device/id";

}  // namespace

SettingsStore::SettingsStore() = default;

SettingsStore::SettingsStore(const QString& iniFilePath)
    : settings_(iniFilePath, QSettings::IniFormat) {}

core::AppConfig SettingsStore::load() const {
    const core::AppConfig defaults;
    core::AppConfig config;

    config.pollIntervalSeconds =
        settings_.value(kPollIntervalSeconds, defaults.pollIntervalSeconds).toInt();
    config.startMinimized =
        settings_.value(kStartMinimized, defaults.startMinimized).toBool();
    config.autostartWithWindows =
        settings_.value(kAutostartWithWindows, defaults.autostartWithWindows).toBool();
    config.notifyOnError =
        settings_.value(kNotifyOnError, defaults.notifyOnError).toBool();
    config.notifyOnSyncComplete =
        settings_.value(kNotifyOnSyncComplete, defaults.notifyOnSyncComplete).toBool();
    config.logMaxFileBytes = static_cast<std::size_t>(
        settings_.value(kLogMaxFileBytes,
                        static_cast<qulonglong>(defaults.logMaxFileBytes)).toULongLong());
    config.logMaxFiles = static_cast<std::size_t>(
        settings_.value(kLogMaxFiles,
                        static_cast<qulonglong>(defaults.logMaxFiles)).toULongLong());

    return config.clamped();
}

void SettingsStore::save(const core::AppConfig& config) {
    const core::AppConfig sane = config.clamped();

    settings_.setValue(kPollIntervalSeconds, sane.pollIntervalSeconds);
    settings_.setValue(kStartMinimized, sane.startMinimized);
    settings_.setValue(kAutostartWithWindows, sane.autostartWithWindows);
    settings_.setValue(kNotifyOnError, sane.notifyOnError);
    settings_.setValue(kNotifyOnSyncComplete, sane.notifyOnSyncComplete);
    settings_.setValue(kLogMaxFileBytes, static_cast<qulonglong>(sane.logMaxFileBytes));
    settings_.setValue(kLogMaxFiles, static_cast<qulonglong>(sane.logMaxFiles));
    settings_.sync();
}

QString SettingsStore::deviceId() {
    const QString stored = settings_.value(kDeviceId).toString();
    if (!stored.isEmpty()) {
        return stored;
    }

    // QUuid::createUuid() zieht aus dem Zufallsgenerator des Systems.
    const QString created = QUuid::createUuid().toString(QUuid::WithoutBraces);
    settings_.setValue(kDeviceId, created);
    settings_.sync();
    return created;
}

QString SettingsStore::deviceName() {
    const QString host = QSysInfo::machineHostName();
    // Ein leerer Name wäre in der Geräteliste des Servers nicht zuzuordnen.
    return host.isEmpty() ? QStringLiteral("BaluDesk") : host;
}

}  // namespace baludesk::app
