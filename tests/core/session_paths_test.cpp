#include "core/auth/SessionPaths.h"

#include <gtest/gtest.h>

#include <cstdlib>
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

TEST(SessionPathsTest, StaysAbsoluteWhenLocalAppDataIsMissing) {
    // Der Header sagt „immer absolut" zu. Fehlt LOCALAPPDATA - in einem
    // Dienstkonto oder einer abgespeckten Umgebung durchaus möglich -, darf
    // das Ergebnis nicht am Arbeitsverzeichnis hängen: Oberfläche und
    // Prüfprogramm fänden sonst je nach Startort verschiedene Sitzungen.
#pragma warning(push)
#pragma warning(disable : 4996)
    const char* previous = std::getenv("LOCALAPPDATA");
#pragma warning(pop)
    const std::string saved = previous != nullptr ? previous : "";

    // Ein leerer Wert entfernt die Variable unter Windows.
    ASSERT_EQ(_putenv_s("LOCALAPPDATA", ""), 0);
    const std::string path = defaultSessionPath();
    if (!saved.empty()) {
        ASSERT_EQ(_putenv_s("LOCALAPPDATA", saved.c_str()), 0);
    }

    EXPECT_FALSE(path.empty());
    EXPECT_TRUE(std::filesystem::path(path).is_absolute());
}
