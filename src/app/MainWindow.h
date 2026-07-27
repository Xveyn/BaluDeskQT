#pragma once

#include <QMainWindow>

class QStackedWidget;
class QThread;

namespace baludesk::bridge {
class AuthBridge;
}

namespace baludesk::app {

class CertificateChangedPage;
class CertificatePage;
class ConnectedPage;
class PairingCodePage;
class ServerUrlPage;

// Hauptfenster: ein Stapel von Ansichten und der Eigentümer des
// Arbeiterthreads.
//
// DIES IST DIE EINZIGE STELLE, AN DER EIN THREAD ENTSTEHT. Die AuthBridge
// selbst weiss nichts davon; sie wird hier per moveToThread verschoben, und
// alle Verbindungen laufen von da an über die Ereigniswarteschlange. Wer
// Nebenläufigkeitsfehler sucht, sucht hier.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void buildPages();
    void startAuthThread();
    void wireBridge();

    QStackedWidget* stack_ = nullptr;

    ServerUrlPage* serverUrlPage_ = nullptr;
    CertificatePage* certificatePage_ = nullptr;
    PairingCodePage* pairingCodePage_ = nullptr;
    CertificateChangedPage* certificateChangedPage_ = nullptr;
    ConnectedPage* connectedPage_ = nullptr;

    QThread* authThread_ = nullptr;
    bridge::AuthBridge* bridge_ = nullptr;
};

}  // namespace baludesk::app
