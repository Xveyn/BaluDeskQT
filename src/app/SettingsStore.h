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

    // Die Kennung dieser Installation, wie sie beim Paaren als device_id
    // mitgeht. Beim ersten Aufruf erzeugt und abgelegt, danach unverändert.
    //
    // Absichtlich NICHT in der Sitzung: der Server führt daraus seine
    // Geräteliste in der Web-Oberfläche. Läge sie in der Sitzung, bekäme
    // jedes Neupaaren einen neuen Eintrag, und das Aufräumen wäre Handarbeit
    // im Browser. Die Kennung gehört der Installation, nicht der Bindung an
    // einen Server - und überlebt deshalb das Abmelden.
    //
    // Nicht const: der erste Aufruf schreibt.
    [[nodiscard]] QString deviceId();

    // Der Name, unter dem das Gerät in der Web-Oberfläche erscheint.
    [[nodiscard]] static QString deviceName();

private:
    QSettings settings_;
};

}  // namespace baludesk::app
