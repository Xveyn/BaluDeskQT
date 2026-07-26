#include "core/util/Logger.h"

#include <spdlog/sinks/rotating_file_sink.h>

#include <memory>
#include <mutex>

namespace baludesk::core {
namespace {

constexpr const char* kLoggerName = "baludesk";
constexpr const char* kLogFileName = "baludesk.log";

std::shared_ptr<spdlog::logger> g_logger;
std::string g_logFilePath;
std::mutex g_mutex;

}  // namespace

void Logger::init(const std::string& logDirectory,
                  std::size_t maxFileBytes,
                  std::size_t maxFiles) {
    std::lock_guard<std::mutex> lock(g_mutex);

    // Der erste Aufruf gewinnt. Ein zweiter würde sonst mitten im Betrieb
    // die Senke unter laufenden Threads austauschen.
    if (g_logger) {
        return;
    }

    g_logFilePath = logDirectory + "/" + kLogFileName;

    auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        g_logFilePath, maxFileBytes, maxFiles);

    g_logger = std::make_shared<spdlog::logger>(kLoggerName, std::move(sink));
    g_logger->set_level(spdlog::level::debug);
    g_logger->flush_on(spdlog::level::warn);
    g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_logger) {
        g_logger->flush();
        g_logger.reset();
    }
    g_logFilePath.clear();
}

bool Logger::isInitialized() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return static_cast<bool>(g_logger);
}

std::string Logger::logFilePath() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_logFilePath;
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_logger) {
        g_logger->flush();
    }
}

spdlog::logger* Logger::raw() {
    // Bewusst ohne Sperre: init() läuft beim Programmstart, bevor weitere
    // Threads existieren, und danach ändert sich der Zeiger nicht mehr.
    return g_logger.get();
}

}  // namespace baludesk::core
