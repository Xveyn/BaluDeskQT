#pragma once

#include <cstddef>

namespace baludesk::core {

// Einstellungswerte der Anwendung als schlichte Datenstruktur.
//
// Bewusst ohne Qt: der Kern kennt Qt nicht. Die Persistenz übernimmt
// baludesk::app::SettingsStore, der diese Struktur liest und schreibt.
struct AppConfig {
    // Abstand zwischen zwei Abfragen entfernter Änderungen.
    int pollIntervalSeconds = 60;

    // Beim Start direkt in den Infobereich minimieren.
    bool startMinimized = false;

    // Mit Windows automatisch starten.
    bool autostartWithWindows = false;

    // Benachrichtigung bei Fehlern und Konflikten.
    bool notifyOnError = true;

    // Benachrichtigung bei abgeschlossenen Sync-Läufen. Standardmäßig aus,
    // weil es sonst schnell lästig wird.
    bool notifyOnSyncComplete = false;

    // Obergrenze je Protokolldatei in Bytes.
    std::size_t logMaxFileBytes = std::size_t{5} * 1024 * 1024;

    // Anzahl aufbewahrter älterer Protokolldateien.
    std::size_t logMaxFiles = 3;

    static constexpr int kMinPollIntervalSeconds = 15;
    static constexpr int kMaxPollIntervalSeconds = 3600;
    static constexpr std::size_t kMinLogMaxFileBytes = std::size_t{64} * 1024;
    static constexpr std::size_t kMaxLogMaxFileBytes = std::size_t{50} * 1024 * 1024;
    static constexpr std::size_t kMaxLogFiles = 20;

    // Alle Werte auf ihre gültigen Bereiche zurechtstutzen.
    // Idempotent: clamped().clamped() == clamped().
    [[nodiscard]] AppConfig clamped() const;
};

bool operator==(const AppConfig& lhs, const AppConfig& rhs);
bool operator!=(const AppConfig& lhs, const AppConfig& rhs);

}  // namespace baludesk::core
