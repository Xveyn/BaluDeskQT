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

TEST(AppConfigTest, ConstantsMatchTheSpecifiedLiterals) {
    // Verankert die Grenzwerte gegen die Spezifikation. Ohne diesen Test
    // pruefen alle uebrigen nur gegen die Konstanten des Pruflings selbst.
    EXPECT_EQ(AppConfig::kMinPollIntervalSeconds, 15);
    EXPECT_EQ(AppConfig::kMaxPollIntervalSeconds, 3600);
    EXPECT_EQ(AppConfig::kMinLogMaxFileBytes, std::size_t{65536});
    EXPECT_EQ(AppConfig::kMaxLogMaxFileBytes, std::size_t{52428800});
    EXPECT_EQ(AppConfig::kMaxLogFiles, std::size_t{20});
}

TEST(AppConfigTest, LeavesLowerBoundaryValuesUntouched) {
    AppConfig config;
    config.pollIntervalSeconds = 15;
    config.logMaxFileBytes = std::size_t{65536};
    config.logMaxFiles = 1;

    const AppConfig result = config.clamped();
    EXPECT_EQ(result.pollIntervalSeconds, 15);
    EXPECT_EQ(result.logMaxFileBytes, std::size_t{65536});
    EXPECT_EQ(result.logMaxFiles, std::size_t{1});
}

TEST(AppConfigTest, LeavesUpperBoundaryValuesUntouched) {
    AppConfig config;
    config.pollIntervalSeconds = 3600;
    config.logMaxFileBytes = std::size_t{52428800};
    config.logMaxFiles = 20;

    const AppConfig result = config.clamped();
    EXPECT_EQ(result.pollIntervalSeconds, 3600);
    EXPECT_EQ(result.logMaxFileBytes, std::size_t{52428800});
    EXPECT_EQ(result.logMaxFiles, std::size_t{20});
}

TEST(AppConfigTest, ClampsJustOutsideTheBoundaries) {
    AppConfig config;

    config.pollIntervalSeconds = 14;
    EXPECT_EQ(config.clamped().pollIntervalSeconds, 15);

    config.pollIntervalSeconds = 3601;
    EXPECT_EQ(config.clamped().pollIntervalSeconds, 3600);
}

TEST(AppConfigTest, ClampsLogFileSizeAboveMaximum) {
    AppConfig config;
    config.logMaxFileBytes = std::size_t{500} * 1024 * 1024;
    EXPECT_EQ(config.clamped().logMaxFileBytes, std::size_t{52428800});
}

TEST(AppConfigTest, InequalityIsTheNegationOfEquality) {
    AppConfig left;
    AppConfig right;
    EXPECT_FALSE(left != right);

    right.pollIntervalSeconds = 120;
    EXPECT_TRUE(left != right);
}
