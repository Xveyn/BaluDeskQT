#include "app/SettingsStore.h"

#include <gtest/gtest.h>

#include <QSettings>
#include <QString>
#include <QTemporaryDir>

using baludesk::app::SettingsStore;
using baludesk::core::AppConfig;

TEST(SettingsStoreTest, ReturnsDefaultsWhenFileIsEmpty) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    const SettingsStore store(directory.filePath(QStringLiteral("settings.ini")));

    EXPECT_TRUE(store.load() == AppConfig{});
}

TEST(SettingsStoreTest, RoundTripsAllValues) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.ini"));

    AppConfig written;
    written.pollIntervalSeconds = 120;
    written.startMinimized = true;
    written.autostartWithWindows = true;
    written.notifyOnError = false;
    written.notifyOnSyncComplete = true;
    written.logMaxFileBytes = std::size_t{2} * 1024 * 1024;
    written.logMaxFiles = 5;

    {
        SettingsStore store(path);
        store.save(written);
    }

    const SettingsStore reopened(path);
    EXPECT_TRUE(reopened.load() == written);
}

TEST(SettingsStoreTest, ClampsOutOfRangeValuesFromFile) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.ini"));

    {
        QSettings raw(path, QSettings::IniFormat);
        raw.setValue(QStringLiteral("sync/pollIntervalSeconds"), 1);
        raw.setValue(QStringLiteral("logging/maxFiles"), 999);
        raw.sync();
    }

    const SettingsStore store(path);
    const AppConfig loaded = store.load();

    EXPECT_EQ(loaded.pollIntervalSeconds, AppConfig::kMinPollIntervalSeconds);
    EXPECT_EQ(loaded.logMaxFiles, AppConfig::kMaxLogFiles);
}

TEST(SettingsStoreTest, SaveWritesClampedValues) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.ini"));

    AppConfig unreasonable;
    unreasonable.pollIntervalSeconds = 999999;

    {
        SettingsStore store(path);
        store.save(unreasonable);
    }

    const QSettings raw(path, QSettings::IniFormat);
    EXPECT_EQ(raw.value(QStringLiteral("sync/pollIntervalSeconds")).toInt(),
              AppConfig::kMaxPollIntervalSeconds);
}
