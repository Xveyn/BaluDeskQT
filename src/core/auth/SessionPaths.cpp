#include "core/auth/SessionPaths.h"

#include <cstdlib>
#include <filesystem>
#include <system_error>

namespace baludesk::core {

std::string defaultSessionPath() {
    // std::getenv gilt bei MSVC unter /W4 als „unsicher" (C4996) und würde die
    // Warnungsfreiheit des Builds verletzen. Hier bewusst und begrenzt
    // unterdrückt statt projektweit, da nur diese eine Stelle betroffen ist.
#pragma warning(push)
#pragma warning(disable : 4996)
    const char* localAppData = std::getenv("LOCALAPPDATA");
#pragma warning(pop)

    std::filesystem::path base;
    if (localAppData != nullptr && *localAppData != '\0') {
        base = localAppData;
    }

    // Der Header sagt „immer absolut" zu. Das frühere Ausweichen auf "."
    // brach diese Zusage still: das Ergebnis hing dann am Arbeitsverzeichnis,
    // und Oberfläche und Prüfprogramm fänden je nach Startort verschiedene
    // Sitzungen - der Fehlermodus wäre „nicht gepaart" ohne jede Meldung.
    if (base.empty() || !base.is_absolute()) {
        std::error_code error;
        base = std::filesystem::temp_directory_path(error);
        if (error || base.empty()) {
            base = std::filesystem::current_path(error);
        }
    }

    return (base / "BaluHost" / "BaluDesk" / "session.json").string();
}

}  // namespace baludesk::core
