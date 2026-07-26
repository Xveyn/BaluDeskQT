#include "core/util/Logger.h"

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <string>
#include <thread>

namespace fs = std::filesystem;
using baludesk::core::Logger;

namespace {

fs::path makeEmptyTempDir(const std::string& name) {
    const fs::path directory = fs::temp_directory_path() / ("baludesk_logger_test_" + name);
    fs::remove_all(directory);
    fs::create_directories(directory);
    return directory;
}

std::size_t countRegularFiles(const fs::path& directory) {
    std::size_t count = 0;
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            ++count;
        }
    }
    return count;
}

}  // namespace

class LoggerTest : public ::testing::Test {
protected:
    void TearDown() override { Logger::shutdown(); }
};

TEST_F(LoggerTest, CreatesLogFileOnInit) {
    const fs::path directory = makeEmptyTempDir("create");

    Logger::init(directory.string(), 1024 * 1024, 3);
    Logger::info("Testeintrag {}", 42);
    Logger::flush();

    EXPECT_TRUE(Logger::isInitialized());
    EXPECT_TRUE(fs::exists(directory / "baludesk.log"));
}

TEST_F(LoggerTest, ReportsLogFilePath) {
    const fs::path directory = makeEmptyTempDir("path");

    Logger::init(directory.string(), 1024 * 1024, 3);

    EXPECT_EQ(Logger::logFilePath(), directory.string() + "/baludesk.log");
}

TEST_F(LoggerTest, RotatesAndKeepsFileCountBounded) {
    const fs::path directory = makeEmptyTempDir("rotate");
    constexpr std::size_t kMaxBytes = 1024;
    constexpr std::size_t kMaxFiles = 3;

    Logger::init(directory.string(), kMaxBytes, kMaxFiles);

    const std::string filler(200, 'x');
    for (int i = 0; i < 200; ++i) {
        Logger::info("{} {}", i, filler);
    }
    Logger::flush();

    const std::size_t fileCount = countRegularFiles(directory);

    // Untere Schranke: es muss überhaupt rotiert worden sein.
    EXPECT_GE(fileCount, std::size_t{2}) << "Es wurde nicht rotiert";

    // Obere Schranke bewusst als kMaxFiles + 1 formuliert: spdlog hält die
    // aktive Datei plus maxFiles rotierte Stände. Der Test prüft, dass die
    // Zahl gedeckelt ist, nicht die genaue Zählweise von spdlog.
    EXPECT_LE(fileCount, kMaxFiles + 1) << "Mehr Dateien als die Obergrenze erlaubt";
}

TEST_F(LoggerTest, SecondInitDoesNotReplaceFirst) {
    const fs::path first = makeEmptyTempDir("first");
    const fs::path second = makeEmptyTempDir("second");

    Logger::init(first.string(), 1024 * 1024, 3);
    Logger::init(second.string(), 1024 * 1024, 3);

    EXPECT_EQ(Logger::logFilePath(), first.string() + "/baludesk.log");
}

TEST_F(LoggerTest, LoggingBeforeInitDoesNotCrash) {
    Logger::shutdown();
    EXPECT_FALSE(Logger::isInitialized());

    Logger::info("Eintrag ohne Initialisierung {}", 1);
    Logger::error("Fehler ohne Initialisierung");

    SUCCEED();
}

TEST_F(LoggerTest, LoggingSurvivesConcurrentShutdown) {
    const fs::path directory = makeEmptyTempDir("race");
    Logger::init(directory.string(), 1024 * 1024, 3);

    std::atomic<bool> stop{false};
    std::thread writer([&stop] {
        while (!stop.load(std::memory_order_relaxed)) {
            Logger::info("Eintrag aus dem Schreiberthread");
        }
    });

    for (int round = 0; round < 200; ++round) {
        Logger::shutdown();
        Logger::init(directory.string(), 1024 * 1024, 3);
    }

    stop.store(true, std::memory_order_relaxed);
    writer.join();

    EXPECT_TRUE(Logger::isInitialized());
    EXPECT_EQ(Logger::logFilePath(), directory.string() + "/baludesk.log");
}
