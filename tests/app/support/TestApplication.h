#pragma once

#include <QApplication>

namespace baludesk::app::test {

// Die eine Anwendungsinstanz für alle Tests dieser ausführbaren Datei.
//
// Eine zweite anzulegen ist in Qt ein fataler Fehler, deshalb steht sie hier
// einmal statisch und nicht in jeder Testdatei erneut. Es ist eine
// QApplication und nicht die kleinere QCoreApplication, weil jedes QWidget
// eine verlangt - auch die Tests, die nur die Bridge anfassen, teilen sich
// diese Instanz und kommen mit ihr aus.
inline QApplication& application() {
    static int argc = 1;
    static char argName[] = "baludesk_app_tests";
    static char* argv[] = {argName, nullptr};
    static QApplication instance(argc, argv);
    return instance;
}

}  // namespace baludesk::app::test
