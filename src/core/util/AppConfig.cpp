#include "core/util/AppConfig.h"

#include <algorithm>

namespace baludesk::core {

AppConfig AppConfig::clamped() const {
    AppConfig result = *this;

    result.pollIntervalSeconds =
        std::clamp(result.pollIntervalSeconds, kMinPollIntervalSeconds, kMaxPollIntervalSeconds);

    result.logMaxFileBytes =
        std::clamp(result.logMaxFileBytes, kMinLogMaxFileBytes, kMaxLogMaxFileBytes);

    result.logMaxFiles =
        std::clamp(result.logMaxFiles, kMinLogFiles, kMaxLogFiles);

    return result;
}

bool operator==(const AppConfig& lhs, const AppConfig& rhs) {
    return lhs.pollIntervalSeconds == rhs.pollIntervalSeconds
        && lhs.startMinimized == rhs.startMinimized
        && lhs.autostartWithWindows == rhs.autostartWithWindows
        && lhs.notifyOnError == rhs.notifyOnError
        && lhs.notifyOnSyncComplete == rhs.notifyOnSyncComplete
        && lhs.logMaxFileBytes == rhs.logMaxFileBytes
        && lhs.logMaxFiles == rhs.logMaxFiles;
}

bool operator!=(const AppConfig& lhs, const AppConfig& rhs) {
    return !(lhs == rhs);
}

}  // namespace baludesk::core
