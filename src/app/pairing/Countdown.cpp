#include "app/pairing/Countdown.h"

namespace baludesk::app {

QString formatRemaining(int seconds) {
    const int clamped = seconds > 0 ? seconds : 0;
    const int minutes = clamped / 60;
    const int remainder = clamped % 60;

    // Die Sekunden zweistellig, die Minuten nicht: „9:41", nicht „09:41".
    return QStringLiteral("%1:%2").arg(minutes).arg(remainder, 2, 10, QLatin1Char('0'));
}

}  // namespace baludesk::app
