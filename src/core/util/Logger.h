#pragma once

#include <spdlog/spdlog.h>

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

namespace baludesk::core {

// Protokollierung in eine rotierende Datei mit fester Obergrenze.
//
// init() läuft einmal beim Programmstart, shutdown() beim Beenden.
// Protokollaufrufe (debug/info/warn/error) dürfen dazu jederzeit nebenläufig
// stattfinden - auch während init() oder shutdown() gerade laufen. Weitere
// Aufrufe von init() nach dem ersten sind wirkungslos - der erste gewinnt.
// Aufrufe vor init() sind erlaubt und verpuffen still, damit ein vergessener
// Aufruf nicht zum Absturz führt.
class Logger {
public:
    static void init(const std::string& logDirectory,
                     std::size_t maxFileBytes,
                     std::size_t maxFiles);

    static void shutdown();

    [[nodiscard]] static bool isInitialized();

    // Vollständiger Pfad der aktiven Protokolldatei, leer wenn nicht
    // initialisiert. Das Trennzeichen zwischen Verzeichnis und Dateiname ist
    // immer ein Schrägstrich; Trennzeichen innerhalb des übergebenen
    // Verzeichnisanteils werden unverändert übernommen.
    [[nodiscard]] static std::string logFilePath();

    static void flush();

    template <typename... Args>
    static void debug(spdlog::format_string_t<Args...> format, Args&&... args) {
        if (const auto logger = handle()) {
            logger->debug(format, std::forward<Args>(args)...);
        }
    }

    template <typename... Args>
    static void info(spdlog::format_string_t<Args...> format, Args&&... args) {
        if (const auto logger = handle()) {
            logger->info(format, std::forward<Args>(args)...);
        }
    }

    template <typename... Args>
    static void warn(spdlog::format_string_t<Args...> format, Args&&... args) {
        if (const auto logger = handle()) {
            logger->warn(format, std::forward<Args>(args)...);
        }
    }

    template <typename... Args>
    static void error(spdlog::format_string_t<Args...> format, Args&&... args) {
        if (const auto logger = handle()) {
            logger->error(format, std::forward<Args>(args)...);
        }
    }

private:
    // Liefert eine Kopie des Zeigers unter der Sperre. Die Kopie hält das
    // Logger-Objekt für die Dauer des Aufrufs am Leben, auch wenn parallel
    // shutdown() läuft - sonst schriebe der Aufrufer in zerstörten Speicher.
    static std::shared_ptr<spdlog::logger> handle();
};

}  // namespace baludesk::core
