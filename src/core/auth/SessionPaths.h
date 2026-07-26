#pragma once

#include <string>

namespace baludesk::core {

// Der eine verbindliche Ort der abgelegten Sitzung.
//
// Unter Windows: %LOCALAPPDATA%/BaluHost/BaluDesk/session.json.
//
// Diese Funktion ist die einzige Quelle der Wahrheit für diesen Pfad. Sowohl
// tools/probe als auch die spätere Oberfläche müssen ihn hier abrufen statt
// ihn selbst herzuleiten - sonst stimmen beide nur zufällig überein, und ein
// Auseinanderlaufen bliebe unbemerkt, bis jemand eine bereits gepaarte
// Sitzung nicht wiederfindet.
//
// Insbesondere QStandardPaths::AppDataLocation ist KEIN Ersatz dafür: das
// löst unter Windows nach Roaming auf (%APPDATA%/BaluHost/BaluDesk), einem
// anderen Verzeichnis als hier. Eine Oberfläche, die diese Funktion
// übersieht und stattdessen AppDataLocation verwendet, würde eine korrekt
// gepaarte Sitzung schlicht nicht finden - ohne jede Fehlermeldung.
[[nodiscard]] std::string defaultSessionPath();

}  // namespace baludesk::core
