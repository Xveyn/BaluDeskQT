#pragma once

#include <QMainWindow>

namespace baludesk::app {

// Hauptfenster der Anwendung. In Stufe 0 bewusst leer - es dient nur dem
// Nachweis, dass Qt korrekt eingebunden ist und die Anwendung startet.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
};

}  // namespace baludesk::app
