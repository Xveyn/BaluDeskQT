#pragma once

#include <QSettings>
#include <QString>

#include "core/util/AppConfig.h"

namespace baludesk::app {

// Persistenz der Einstellungen. Übersetzt zwischen QSettings und der
// Qt-freien Struktur core::AppConfig.
//
// load() liefert immer zurechtgestutzte Werte, save() schreibt immer
// zurechtgestutzte Werte - eine von Hand verfremdete Datei kann die
// Anwendung damit nicht in einen unsinnigen Zustand bringen.
class SettingsStore {
public:
    // Standardort von QSettings. Verlangt, dass Organisations- und
    // Anwendungsname vorher über QCoreApplication gesetzt wurden.
    SettingsStore();

    // Feste INI-Datei. Wird von den Tests benutzt.
    explicit SettingsStore(const QString& iniFilePath);

    [[nodiscard]] core::AppConfig load() const;

    void save(const core::AppConfig& config);

private:
    mutable QSettings settings_;
};

}  // namespace baludesk::app
