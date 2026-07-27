#include "app/pairing/Countdown.h"

#include <gtest/gtest.h>

#include <QString>

using baludesk::app::formatRemaining;

TEST(CountdownTest, FormatsMinutesAndSecondsWithATwoDigitSecond) {
    EXPECT_EQ(formatRemaining(600), QStringLiteral("10:00"));
    EXPECT_EQ(formatRemaining(581), QStringLiteral("9:41"));
    EXPECT_EQ(formatRemaining(65), QStringLiteral("1:05"));
    // Ohne die führende Null läse sich 1:05 als 1:5.
    EXPECT_EQ(formatRemaining(9), QStringLiteral("0:09"));
}

TEST(CountdownTest, NeverShowsANegativeTime) {
    // Der Zeitgeber der Oberfläche und die Gültigkeit des Codes laufen
    // unabhängig voneinander. Läuft die Anzeige über das Ende hinaus, soll
    // sie stehenbleiben statt "-0:03" zu zeigen.
    EXPECT_EQ(formatRemaining(0), QStringLiteral("0:00"));
    EXPECT_EQ(formatRemaining(-3), QStringLiteral("0:00"));
}

TEST(CountdownTest, KeepsCountingBeyondAnHour) {
    // Der Server gibt heute 600 Sekunden vor. Ein grösserer Wert darf die
    // Anzeige nicht in die Irre führen.
    EXPECT_EQ(formatRemaining(3661), QStringLiteral("61:01"));
}
