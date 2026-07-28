#include "app/MainWindow.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QLineEdit>
#include <QTemporaryDir>

#include <memory>
#include <string>

#include "app/pairing/ServerUrlPage.h"
#include "app/support/TestApplication.h"
#include "bridge/AuthBridge.h"
#include "core/support/FakeSecretStore.h"
#include "core/support/FakeTransport.h"

using baludesk::app::MainWindow;
using baludesk::app::ServerUrlPage;
using baludesk::app::test::application;
using baludesk::bridge::AuthBridge;
using baludesk::core::test::FakeSecretStore;
using baludesk::core::test::FakeTransport;

namespace {

// Eine Bridge auf Fakes: ein Testlauf fasst weder den echten
// Anmeldeinformationsspeicher noch das Netz an.
AuthBridge* fakeBridge(const QTemporaryDir& directory,
                       const std::shared_ptr<FakeTransport>& transport) {
    return new AuthBridge(transport,
                          std::make_unique<FakeSecretStore>(),
                          directory.filePath(QStringLiteral("session.json")).toStdString(),
                          QStringLiteral("geraet-1"),
                          QStringLiteral("Svens Rechner"));
}

// Wartet, bis die Antwort aus dem Arbeiterthread im GUI-Thread angekommen und
// abgearbeitet ist. Ohne feste Wartezeit: es wird auf das Ergebnis gewartet,
// nicht auf die Uhr.
bool waitForPage(const MainWindow& window, int milliseconds) {
    QElapsedTimer clock;
    clock.start();
    while (clock.elapsed() < milliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        if (qobject_cast<ServerUrlPage*>(window.currentPage()) != nullptr) {
            return true;
        }
    }
    return false;
}

}  // namespace

TEST(MainWindowTest, OffersNoServerAddressBeforeTheBridgeHasAnswered) {
    application();
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    auto transport = std::make_shared<FakeTransport>();

    MainWindow window(fakeBridge(directory, transport));

    // Bis hierher hat der GUI-Thread keine Ereignisse abgearbeitet - genau
    // der Zustand, in dem der Nutzer das Fenster nach dem Start zuerst sieht.
    // Bei einer gültigen Sitzung und schlafendem Server dauert er bis zu
    // 30 Sekunden.
    //
    // Stünde hier die Eingabeseite, forderte die Anwendung zum Neupaaren auf,
    // obwohl sie gepaart ist (Spec Abschnitt 10) - und wer die Adresse
    // eintippt und die Eingabetaste drückt, überschreibt am Ende eine intakte
    // Sitzung und verbrennt einen Gerätecode.
    EXPECT_EQ(qobject_cast<ServerUrlPage*>(window.currentPage()), nullptr);
    EXPECT_TRUE(window.currentPage()->findChildren<QLineEdit*>().isEmpty());
    EXPECT_TRUE(transport->calls.empty());
}

TEST(MainWindowTest, LeavesTheStartingPageWhenNoSessionIsStored) {
    application();
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    auto transport = std::make_shared<FakeTransport>();

    MainWindow window(fakeBridge(directory, transport));

    // Der Startzustand darf keine Sackgasse sein: ohne abgelegte Sitzung
    // meldet die Bridge noSession, und genau dann gehört die Eingabeseite
    // hin.
    EXPECT_TRUE(waitForPage(window, 10000));
}
