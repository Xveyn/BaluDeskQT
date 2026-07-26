#pragma once

#include <spdlog/spdlog.h>

#include <cstddef>
#include <string>
#include <utility>

namespace baludesk::core {

// Protokollierung in eine rotierende Datei mit fester Obergrenze.
//
// init() muss einmal beim Programmstart aufgerufen werden, bevor weitere
// Threads laufen. Weitere Aufrufe von init() sind wirkungslos - der erste
// gewinnt. Aufrufe vor init() sind erlaubt und verpuffen still, damit ein
// vergessener Aufruf nicht zum Absturz führt.
class Logger {
public:
    static void init(const std::string& logDirectory,
                     std::size_t maxFileBytes,
                     std::size_t maxFiles);

    static void shutdown();

    [[nodiscard]] static bool isInitialized();

    // Vollständiger Pfad der aktiven Protokolldatei, leer wenn nicht
    // initialisiert. Die Trennzeichen sind immer Schrägstriche.
    [[nodiscard]] static std::string logFilePath();

    static void flush();

    template <typename... Args>
    static void debug(spdlog::format_string_t<Args...> format, Args&&... args) {
        if (auto* logger = raw()) {
            logger->debug(format, std::forward<Args>(args)...);
        }
    }

    template <typename... Args>
    static void info(spdlog::format_string_t<Args...> format, Args&&... args) {
        if (auto* logger = raw()) {
            logger->info(format, std::forward<Args>(args)...);
        }
    }

    template <typename... Args>
    static void warn(spdlog::format_string_t<Args...> format, Args&&... args) {
        if (auto* logger = raw()) {
            logger->warn(format, std::forward<Args>(args)...);
        }
    }

    template <typename... Args>
    static void error(spdlog::format_string_t<Args...> format, Args&&... args) {
        if (auto* logger = raw()) {
            logger->error(format, std::forward<Args>(args)...);
        }
    }

private:
    static spdlog::logger* raw();
};

}  // namespace baludesk::core
