#include "app/pairing/CertificatePage.h"

#include <gtest/gtest.h>

#include <QPushButton>
#include <QSignalSpy>
#include <QString>

#include "app/support/TestApplication.h"

using baludesk::app::CertificatePage;
using baludesk::app::test::application;

namespace {

struct Buttons {
    QPushButton* confirm = nullptr;
    QPushButton* reject = nullptr;
};

// Der Abbruchknopf trägt immer denselben Text, der Bestätigungsknopf nicht -
// er heisst während des Aufrufs anders. Deshalb wird über den Abbruch
// unterschieden und nicht über die Anlegereihenfolge, die niemand zusichert.
Buttons buttonsOf(const CertificatePage& page) {
    Buttons found;
    for (QPushButton* button : page.findChildren<QPushButton*>()) {
        if (button->text() == QStringLiteral("Abbrechen")) {
            found.reject = button;
        } else {
            found.confirm = button;
        }
    }
    return found;
}

}  // namespace

TEST(CertificatePageTest, ASecondClickCannotStartASecondPairing) {
    application();
    CertificatePage page;
    page.showCertificate(QStringLiteral("AB:CD:EF"), QStringLiteral("self signed certificate"));

    const Buttons buttons = buttonsOf(page);
    ASSERT_NE(buttons.confirm, nullptr);
    ASSERT_NE(buttons.reject, nullptr);

    QSignalSpy confirmed(&page, &CertificatePage::confirmed);
    buttons.confirm->click();
    buttons.confirm->click();

    // Hinter dem ersten Klick steht ein Aufruf, der im Arbeiterthread bis zu
    // 30 Sekunden blockiert. Ein zweiter Klick reihte sich dahinter ein,
    // erzeugte einen zweiten Gerätecode und verbrauchte die Grenze von drei
    // Anfragen je Minute - der Nutzer sperrte sich selbst aus.
    EXPECT_EQ(confirmed.count(), 1);
    EXPECT_FALSE(buttons.confirm->isEnabled());
    EXPECT_FALSE(buttons.reject->isEnabled());
}

TEST(CertificatePageTest, CancellingIsJustAsFinalAsConfirming) {
    application();
    CertificatePage page;
    page.showCertificate(QStringLiteral("AB:CD:EF"), QStringLiteral("self signed certificate"));

    const Buttons buttons = buttonsOf(page);
    ASSERT_NE(buttons.confirm, nullptr);
    ASSERT_NE(buttons.reject, nullptr);

    QSignalSpy rejected(&page, &CertificatePage::rejected);
    buttons.reject->click();
    buttons.reject->click();

    EXPECT_EQ(rejected.count(), 1);
    EXPECT_FALSE(buttons.confirm->isEnabled());
    EXPECT_FALSE(buttons.reject->isEnabled());
}

TEST(CertificatePageTest, ShowingTheCertificateAgainMakesThePageUsableAgain) {
    application();
    CertificatePage page;
    page.showCertificate(QStringLiteral("AB:CD:EF"), QStringLiteral("self signed certificate"));

    const Buttons buttons = buttonsOf(page);
    ASSERT_NE(buttons.confirm, nullptr);
    ASSERT_NE(buttons.reject, nullptr);

    buttons.confirm->click();
    ASSERT_FALSE(buttons.confirm->isEnabled());

    // Der einzige Weg zurück auf diese Seite. Bliebe sie gesperrt, wäre der
    // zweite Anlauf eine Sackgasse mit zwei toten Knöpfen.
    page.showCertificate(QStringLiteral("12:34:56"), QStringLiteral("self signed certificate"));

    EXPECT_TRUE(buttons.confirm->isEnabled());
    EXPECT_TRUE(buttons.reject->isEnabled());
}
