#include "app/QtLogBridge.h"

#include <gtest/gtest.h>

#include <QDebug>
#include <QString>
#include <QTemporaryDir>

#include <filesystem>
#include <fstream>
#include <string>

#include "core/util/Logger.h"

namespace fs = std::filesystem;
using baludesk::core::Logger;

namespace {

std::string readWholeFile(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

}  // namespace

class QtLogBridgeTest : public ::testing::Test {
protected:
    void TearDown() override { Logger::shutdown(); }
};

TEST_F(QtLogBridgeTest, ForwardsQtWarningsIntoTheLogFile) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    Logger::init(directory.path().toStdString(), 1024 * 1024, 3);
    baludesk::app::installQtLogBridge();

    qWarning() << "Testmeldung aus Qt";
    Logger::flush();

    const std::string contents = readWholeFile(Logger::logFilePath());
    EXPECT_NE(contents.find("Testmeldung aus Qt"), std::string::npos);
    EXPECT_NE(contents.find("[warning]"), std::string::npos);
}

TEST_F(QtLogBridgeTest, ForwardsQtCriticalAsError) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    Logger::init(directory.path().toStdString(), 1024 * 1024, 3);
    baludesk::app::installQtLogBridge();

    qCritical() << "Schwerer Qt-Fehler";
    Logger::flush();

    const std::string contents = readWholeFile(Logger::logFilePath());
    EXPECT_NE(contents.find("Schwerer Qt-Fehler"), std::string::npos);
    EXPECT_NE(contents.find("[error]"), std::string::npos);
}
