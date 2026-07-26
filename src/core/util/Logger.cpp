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

    const std::string logFilePath = logDirectory + "/" + kLogFileName;

    // Der Konstruktor der Senke wirft spdlog::spdlog_ex, wenn die Datei
    // nicht angelegt werden kann (Verzeichnis fehlt, Berechtigung fehlt,
    // Datenträger voll). Logger::init() darf dabei nicht mitreißen - das
    // widerspräche dem in der Klassenbeschreibung dokumentierten
    // Versprechen, dass Protokollierung die Anwendung niemals zum Absturz
    // bringt. g_logFilePath bleibt in diesem Fall leer statt einen Pfad zu
    // zeigen, unter dem gar keine Datei existiert.
    try {
        auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            logFilePath, maxFileBytes, maxFiles);

        auto logger = std::make_shared<spdlog::logger>(kLoggerName, std::move(sink));
        logger->set_level(spdlog::level::debug);
        logger->flush_on(spdlog::level::warn);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

        g_logger = std::move(logger);
        g_logFilePath = logFilePath;
    } catch (const spdlog::spdlog_ex&) {
        g_logger.reset();
        g_logFilePath.clear();
    }
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

std::shared_ptr<spdlog::logger> Logger::handle() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_logger;
}

}  // namespace baludesk::core
