#include "app/pairing/CertificateChangedPage.h"

#include <gtest/gtest.h>

#include <QLabel>
#include <QString>
#include <QStringList>

#include "app/support/TestApplication.h"

using baludesk::app::CertificateChangedPage;
using baludesk::app::test::application;

namespace {

// Alles, was die Seite an Text zeigt. Welche Beschriftung den Satz trägt, ist
// Sache der Seite; geprüft wird, was der Nutzer liest.
QString visibleText(const CertificateChangedPage& page) {
    QStringList parts;
    for (const QLabel* label : page.findChildren<QLabel*>()) {
        parts << label->text();
    }
    return parts.join(QLatin1Char('\n'));
}

}  // namespace

TEST(CertificateChangedPageTest, TellsTheTwoCasesApart) {
    application();
    CertificateChangedPage page;

    // Beide Male derselbe Grund und dieselbe Adresse: unterscheiden darf sich
    // der Text allein daran, ob je ein Schlüssel angeheftet war. Der Grund
    // ist der Klartext der Gegenstelle und trägt die Unterscheidung nicht.
    page.showChange(QStringLiteral("https://nas.example.com"),
                    QStringLiteral("certificate verify failed"),
                    true);
    const QString pinned = visibleText(page);

    page.showChange(QStringLiteral("https://nas.example.com"),
                    QStringLiteral("certificate verify failed"),
                    false);
    const QString unpinned = visibleText(page);

    EXPECT_NE(pinned, unpinned);
}

TEST(CertificateChangedPageTest, DoesNotClaimAnExchangeWhenNothingWasEverPinned) {
    application();
    CertificateChangedPage page;

    // Ein Server mit öffentlich vertrauenswürdigem Zertifikat: es wurde beim
    // Verbinden nichts bestätigt und nichts angeheftet, public_key_pin ist
    // leer. Läuft das Zertifikat ab, meldet der Client TrustBroken - von
    // einem ausgetauschten Zertifikat zu sprechen wäre eine falsche Diagnose,
    // und der einzige Ausweg dieser Seite löscht eine intakte Sitzung.
    page.showChange(QStringLiteral("https://nas.example.com"),
                    QStringLiteral("certificate has expired"),
                    false);

    EXPECT_FALSE(visibleText(page).contains(QStringLiteral("anderes Zertifikat")));
}

TEST(CertificateChangedPageTest, NamesTheExchangeWhenAKeyWasPinned) {
    application();
    CertificateChangedPage page;

    page.showChange(QStringLiteral("https://nas.example.com"),
                    QStringLiteral("pinned key mismatch"),
                    true);

    const QString text = visibleText(page);
    EXPECT_TRUE(text.contains(QStringLiteral("anderes Zertifikat")));
    EXPECT_TRUE(text.contains(QStringLiteral("nas.example.com")));
}
