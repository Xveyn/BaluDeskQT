#include "core/util/AppConfig.h"

#include <gtest/gtest.h>

using baludesk::core::AppConfig;

TEST(AppConfigTest, DefaultsAreSane) {
    const AppConfig config;
    EXPECT_EQ(config.pollIntervalSeconds, 60);
    EXPECT_FALSE(config.startMinimized);
    EXPECT_FALSE(config.autostartWithWindows);
    EXPECT_TRUE(config.notifyOnError);
    EXPECT_FALSE(config.notifyOnSyncComplete);
    EXPECT_EQ(config.logMaxFiles, std::size_t{3});
}

TEST(AppConfigTest, ClampsPollIntervalBelowMinimum) {
    AppConfig config;
    config.pollIntervalSeconds = 1;
    EXPECT_EQ(config.clamped().pollIntervalSeconds, AppConfig::kMinPollIntervalSeconds);
}

TEST(AppConfigTest, ClampsPollIntervalAboveMaximum) {
    AppConfig config;
    config.pollIntervalSeconds = 999999;
    EXPECT_EQ(config.clamped().pollIntervalSeconds, AppConfig::kMaxPollIntervalSeconds);
}

TEST(AppConfigTest, LeavesValidPollIntervalAlone) {
    AppConfig config;
    config.pollIntervalSeconds = 120;
    EXPECT_EQ(config.clamped().pollIntervalSeconds, 120);
}

TEST(AppConfigTest, ClampsLogLimits) {
    AppConfig config;
    config.logMaxFileBytes = 1;
    config.logMaxFiles = 999;

    const AppConfig result = config.clamped();
    EXPECT_EQ(result.logMaxFileBytes, AppConfig::kMinLogMaxFileBytes);
    EXPECT_EQ(result.logMaxFiles, AppConfig::kMaxLogFiles);
}

TEST(AppConfigTest, ClampsLogFileCountToAtLeastOne) {
    AppConfig config;
    config.logMaxFiles = 0;
    EXPECT_EQ(config.clamped().logMaxFiles, std::size_t{1});
}

TEST(AppConfigTest, ClampingIsIdempotent) {
    AppConfig config;
    config.pollIntervalSeconds = 5;
    config.logMaxFiles = 999;

    const AppConfig once = config.clamped();
    const AppConfig twice = once.clamped();
    EXPECT_TRUE(once == twice);
}
