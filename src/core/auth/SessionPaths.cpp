#include "core/auth/SessionPaths.h"

#include <cstdlib>

namespace baludesk::core {

std::string defaultSessionPath() {
    // std::getenv gilt bei MSVC unter /W4 als "unsicher" (C4996) und würde
    // die Warnungsfreiheit des Builds verletzen. Hier bewusst und begrenzt
    // unterdrückt statt projektweit, da nur diese eine Stelle betroffen ist.
#pragma warning(push)
#pragma warning(disable : 4996)
    const char* localAppData = std::getenv("LOCALAPPDATA");
#pragma warning(pop)
    const std::string base = localAppData != nullptr ? localAppData : ".";
    return base + "/BaluHost/BaluDesk/session.json";
}

}  // namespace baludesk::core
