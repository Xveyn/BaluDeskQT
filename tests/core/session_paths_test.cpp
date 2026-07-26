#include "core/auth/SessionPaths.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

using baludesk::core::defaultSessionPath;

TEST(SessionPathsTest, ReturnsANonEmptyAbsolutePathEndingInSessionJson) {
    const std::string path = defaultSessionPath();

    EXPECT_FALSE(path.empty());
    EXPECT_TRUE(std::filesystem::path(path).is_absolute());

    const std::string suffix = "session.json";
    ASSERT_GE(path.size(), suffix.size());
    EXPECT_EQ(path.compare(path.size() - suffix.size(), suffix.size(), suffix), 0);
}
