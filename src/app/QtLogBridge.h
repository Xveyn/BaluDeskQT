#pragma once

namespace baludesk::app {

// Leitet Qts eigene Meldungen (qDebug, qWarning, qCritical, qFatal) in den
// Logger um.
//
// Ohne das verschwinden sie spurlos: das Programm wird als WIN32-Anwendung
// gebaut, hat also keine Konsole, und Qt schreibt seine Diagnose sonst nach
// stderr. Genau dort landen aber die Meldungen, die bei Netzwerk- und
// Einstellungsproblemen weiterhelfen.
//
// Darf erst nach Logger::init() aufgerufen werden. Ein Aufruf davor ist
// unschädlich, die Meldungen verpuffen dann still.
void installQtLogBridge();

}  // namespace baludesk::app
