#include "app/MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QStackedWidget>
#include <QThread>

#include <memory>

#include "app/ConnectedPage.h"
#include "app/SettingsStore.h"
#include "app/StartupPage.h"
#include "app/pairing/CertificateChangedPage.h"
#include "app/pairing/CertificatePage.h"
#include "app/pairing/PairingCodePage.h"
#include "app/pairing/ServerUrlPage.h"
#include "bridge/AuthBridge.h"
#include "core/api/CurlTransport.h"
#include "core/auth/SessionPaths.h"
#include "core/util/WindowsCredentialStore.h"

namespace baludesk::app {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    initialise(createBridge());
}

MainWindow::MainWindow(bridge::AuthBridge* bridge, QWidget* parent) : QMainWindow(parent) {
    initialise(bridge);
}

bridge::AuthBridge* MainWindow::createBridge() {
    SettingsStore settings;

    return new bridge::AuthBridge(std::make_shared<core::CurlTransport>(),
                                  std::make_unique<core::WindowsCredentialStore>(),
                                  core::defaultSessionPath(),
                                  settings.deviceId(),
                                  SettingsStore::deviceName());
}

void MainWindow::initialise(bridge::AuthBridge* bridge) {
    setWindowTitle(QStringLiteral("BaluDesk %1").arg(QCoreApplication::applicationVersion()));
    resize(900, 600);

    bridge_ = bridge;

    buildPages();
    startAuthThread();
    wireBridge();

    // Erst jetzt fragen: die Verbindungen müssen stehen, sonst geht die
    // Antwort ins Leere.
    QMetaObject::invokeMethod(bridge_, "restoreSession", Qt::QueuedConnection);
}

QWidget* MainWindow::currentPage() const {
    return stack_->currentWidget();
}

MainWindow::~MainWindow() {
    // Der Thread muss vor dem Fenster enden. deleteLater auf finished sorgt
    // dafür, dass die Bridge in ihrem eigenen Thread stirbt - dort, wo ihre
    // Mitglieder entstanden sind.
    authThread_->quit();
    authThread_->wait();
}

void MainWindow::buildPages() {
    stack_ = new QStackedWidget(this);

    startupPage_ = new StartupPage(this);
    serverUrlPage_ = new ServerUrlPage(this);
    certificatePage_ = new CertificatePage(this);
    pairingCodePage_ = new PairingCodePage(this);
    certificateChangedPage_ = new CertificateChangedPage(this);
    connectedPage_ = new ConnectedPage(this);

    stack_->addWidget(startupPage_);
    stack_->addWidget(serverUrlPage_);
    stack_->addWidget(certificatePage_);
    stack_->addWidget(pairingCodePage_);
    stack_->addWidget(certificateChangedPage_);
    stack_->addWidget(connectedPage_);

    // Bis zur ersten Antwort der Bridge eine eigene Seite statt der
    // Eingabeseite. restoreSession() läuft im Arbeiterthread und kann bei
    // einem schlafenden Server bis zu 30 Sekunden brauchen (in der Abnahme
    // von Hand belegt mit „Connection timed out after 10015 milliseconds").
    // Wer solange „Mit einem Server verbinden" liest, hält sich für
    // ungepaart - und genau diese Aufforderung verbietet Spec Abschnitt 10.
    //
    // Erwogen und verworfen: serverUrlPage_->setBusy(true) von Beginn an. Das
    // sperrt zwar die Eingabe, zeigt aber weiterhin denselben Text, nur
    // ausgegraut - die falsche Auskunft bliebe stehen. Eine eigene Seite sagt
    // stattdessen, was tatsächlich geschieht, und bietet nichts an, was sich
    // hineintippen liesse.
    //
    // Der Startzustand ist keine Sackgasse: restoreSession() meldet genau
    // eines von noSession, sessionEstablished, serverUnreachable,
    // reauthRequired und certificateChanged, und jedes dieser fünf Signale
    // wechselt in wireBridge() die Seite.
    stack_->setCurrentWidget(startupPage_);
    setCentralWidget(stack_);
}

void MainWindow::startAuthThread() {
    authThread_ = new QThread(this);
    bridge_->moveToThread(authThread_);
    connect(authThread_, &QThread::finished, bridge_, &QObject::deleteLater);
    authThread_->start();
}

void MainWindow::wireBridge() {
    // Von der Oberfläche zur Bridge. Weil beide in verschiedenen Threads
    // leben, stellt Qt diese Aufrufe selbsttätig in die Warteschlange - der
    // GUI-Thread wartet nie.
    connect(serverUrlPage_, &ServerUrlPage::connectRequested,
            bridge_, &bridge::AuthBridge::connectToServer);
    connect(certificatePage_, &CertificatePage::confirmed,
            bridge_, &bridge::AuthBridge::confirmCertificate);
    connect(certificatePage_, &CertificatePage::rejected,
            bridge_, &bridge::AuthBridge::rejectCertificate);
    connect(pairingCodePage_, &PairingCodePage::cancelled,
            bridge_, &bridge::AuthBridge::cancelPairing);
    connect(connectedPage_, &ConnectedPage::logoutRequested,
            bridge_, &bridge::AuthBridge::logout);
    connect(certificateChangedPage_, &CertificateChangedPage::repairRequested,
            bridge_, &bridge::AuthBridge::logout);
    connect(certificateChangedPage_, &CertificateChangedPage::quitRequested,
            qApp, &QApplication::quit);

    // Beim Klick auf „Verbinden" den Knopf sperren - der Aufruf kann bis zu
    // 30 Sekunden dauern, und ein zweiter Klick startete einen zweiten
    // Vorgang.
    connect(serverUrlPage_, &ServerUrlPage::connectRequested,
            this, [this]() { serverUrlPage_->setBusy(true); });

    // Von der Bridge zur Oberfläche.
    connect(bridge_, &bridge::AuthBridge::noSession, this, [this]() {
        serverUrlPage_->setBusy(false);
        stack_->setCurrentWidget(serverUrlPage_);
    });

    connect(bridge_, &bridge::AuthBridge::sessionEstablished, this,
            [this](const QString& serverUrl, const QString& username) {
                pairingCodePage_->stop();
                serverUrlPage_->setBusy(false);
                connectedPage_->clearOffline();
                connectedPage_->showConnection(serverUrl, username);
                stack_->setCurrentWidget(connectedPage_);
            });

    // Ein schlafender Server ist laut Spec Abschnitt 10 ein normaler
    // Betriebszustand. Die Sitzung bleibt, also bleibt auch die verbundene
    // Ansicht - nur mit einem Hinweis darüber. Den Nutzer hier zur
    // Serveradresse zurückzuschicken hiesse ihm zu sagen, er sei nicht
    // gepaart, und genau das nennt die Spec im Alltag unbrauchbar.
    connect(bridge_, &bridge::AuthBridge::serverUnreachable, this,
            [this](const QString& serverUrl, const QString& username,
                   const QString& message) {
                pairingCodePage_->stop();
                serverUrlPage_->setBusy(false);
                connectedPage_->showConnection(serverUrl, username);
                connectedPage_->showOffline(message);
                stack_->setCurrentWidget(connectedPage_);
            });

    connect(bridge_, &bridge::AuthBridge::serverUrlRejected,
            serverUrlPage_, &ServerUrlPage::showProblem);

    connect(bridge_, &bridge::AuthBridge::certificateConfirmationRequired, this,
            [this](const QString& fingerprint, const QString& reason) {
                certificatePage_->showCertificate(fingerprint, reason);
                stack_->setCurrentWidget(certificatePage_);
            });

    connect(bridge_, &bridge::AuthBridge::certificateChanged, this,
            [this](const QString& serverUrl, const QString& reason) {
                pairingCodePage_->stop();
                certificateChangedPage_->showChange(serverUrl, reason);
                stack_->setCurrentWidget(certificateChangedPage_);
            });

    connect(bridge_, &bridge::AuthBridge::pairingCodeReady, this,
            [this](const QString& code, const QString& url, int expiresInSeconds) {
                pairingCodePage_->showCode(code, url, expiresInSeconds);
                stack_->setCurrentWidget(pairingCodePage_);
            });

    connect(bridge_, &bridge::AuthBridge::pairingNotice,
            pairingCodePage_, &PairingCodePage::showNotice);

    connect(bridge_, &bridge::AuthBridge::pairingFailed, this,
            [this](const QString& message) {
                pairingCodePage_->stop();
                serverUrlPage_->showProblem(message);
                stack_->setCurrentWidget(serverUrlPage_);
            });

    connect(bridge_, &bridge::AuthBridge::pairingCancelled, this, [this]() {
        pairingCodePage_->stop();
        serverUrlPage_->setBusy(false);
        serverUrlPage_->clearMessages();
        stack_->setCurrentWidget(serverUrlPage_);
    });

    connect(bridge_, &bridge::AuthBridge::reauthRequired, this,
            [this](const QString& message) {
                serverUrlPage_->showNotice(message);
                stack_->setCurrentWidget(serverUrlPage_);
            });

    connect(bridge_, &bridge::AuthBridge::loggedOut, this, [this]() {
        serverUrlPage_->setBusy(false);
        serverUrlPage_->clearMessages();
        stack_->setCurrentWidget(serverUrlPage_);
    });

    connect(bridge_, &bridge::AuthBridge::logoutIncomplete, this,
            [this](const QString& message) {
                // Auch unvollständig abgemeldet heisst abgemeldet - die
                // Ansicht wechselt, die Einschränkung wird benannt.
                serverUrlPage_->setBusy(false);
                serverUrlPage_->showProblem(message);
                stack_->setCurrentWidget(serverUrlPage_);
            });
}

}  // namespace baludesk::app
