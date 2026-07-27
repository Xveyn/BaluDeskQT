#pragma once

#include <QString>

namespace baludesk::app {

// Restlaufzeit als „9:41". Negative Werte ergeben „0:00".
//
// Freistehend statt als Methode der Seite, weil das die einzige Regel der
// Oberfläche ist, die falsch sein kann, ohne dass es auffällt - und weil sie
// so ohne Widget prüfbar bleibt.
[[nodiscard]] QString formatRemaining(int seconds);

}  // namespace baludesk::app
