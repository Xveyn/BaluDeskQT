#include "core/api/ServerUrl.h"

#include <gtest/gtest.h>

#include <string>

using baludesk::core::checkServerUrl;
using baludesk::core::ServerUrlProblem;

TEST(ServerUrlTest, AcceptsPlainHttps) {
    const auto check = checkServerUrl("https://nas.example.com");
    EXPECT_TRUE(check.ok());
    EXPECT_EQ(check.problem, ServerUrlProblem::None);
    EXPECT_EQ(check.normalized, "https://nas.example.com");
}

TEST(ServerUrlTest, AcceptsHttpsWithPortAndPath) {
    const auto check = checkServerUrl("https://nas.example.com:8443");
    EXPECT_TRUE(check.ok());
    EXPECT_EQ(check.normalized, "https://nas.example.com:8443");
}

TEST(ServerUrlTest, StripsTrailingSlashes) {
    EXPECT_EQ(checkServerUrl("https://nas.example.com/").normalized, "https://nas.example.com");
    EXPECT_EQ(checkServerUrl("https://nas.example.com///").normalized, "https://nas.example.com");
}

TEST(ServerUrlTest, TrimsSurroundingWhitespace) {
    const auto check = checkServerUrl("   https://nas.example.com  ");
    EXPECT_TRUE(check.ok());
    EXPECT_EQ(check.normalized, "https://nas.example.com");
}

TEST(ServerUrlTest, AddsHttpsWhenNoSchemeIsGiven) {
    // Wer "nas.example.com" eintippt, meint HTTPS. Das stillschweigend zu
    // ergänzen ist bequem und sicher - die Gegenrichtung wäre es nicht.
    const auto check = checkServerUrl("nas.example.com");
    EXPECT_TRUE(check.ok());
    EXPECT_EQ(check.normalized, "https://nas.example.com");
}

TEST(ServerUrlTest, RejectsEmptyInput) {
    EXPECT_EQ(checkServerUrl("").problem, ServerUrlProblem::Empty);
    EXPECT_EQ(checkServerUrl("    ").problem, ServerUrlProblem::Empty);
    EXPECT_FALSE(checkServerUrl("").ok());
}

TEST(ServerUrlTest, RejectsSchemesOtherThanHttpAndHttps) {
    EXPECT_EQ(checkServerUrl("ftp://nas.example.com").problem,
              ServerUrlProblem::UnsupportedScheme);
    EXPECT_EQ(checkServerUrl("file:///c:/tmp").problem,
              ServerUrlProblem::UnsupportedScheme);
}

TEST(ServerUrlTest, RejectsPlainHttpToARemoteHost) {
    // Der Kern der Regel: über diese Verbindung geht ein Zugangstoken.
    const auto check = checkServerUrl("http://nas.example.com");
    EXPECT_FALSE(check.ok());
    EXPECT_EQ(check.problem, ServerUrlProblem::InsecureScheme);
    EXPECT_FALSE(check.message.empty());
}

TEST(ServerUrlTest, RejectsPlainHttpToARemoteIpAddress) {
    EXPECT_EQ(checkServerUrl("http://192.168.1.50:8000").problem,
              ServerUrlProblem::InsecureScheme);
}

TEST(ServerUrlTest, AllowsPlainHttpToLocalhost) {
    // Ausnahme laut Spec Abschnitt 2.9, damit Entwicklung gegen eine lokale
    // Dev-Instanz möglich bleibt.
    EXPECT_TRUE(checkServerUrl("http://localhost:8000").ok());
    EXPECT_TRUE(checkServerUrl("http://127.0.0.1:8000").ok());
    EXPECT_TRUE(checkServerUrl("http://[::1]:8000").ok());
}

TEST(ServerUrlTest, TreatsLocalhostCaseInsensitively) {
    EXPECT_TRUE(checkServerUrl("http://LOCALHOST:8000").ok());
}

TEST(ServerUrlTest, DoesNotMistakeALookalikeHostForLocalhost) {
    // "localhost.example.com" ist ein fremder Rechner, kein localhost.
    EXPECT_EQ(checkServerUrl("http://localhost.example.com").problem,
              ServerUrlProblem::InsecureScheme);
    EXPECT_EQ(checkServerUrl("http://notlocalhost").problem,
              ServerUrlProblem::InsecureScheme);
    // Auch nicht über die Benutzerangabe hereinzulegen.
    EXPECT_EQ(checkServerUrl("http://localhost@evil.example.com").problem,
              ServerUrlProblem::InsecureScheme);
}

TEST(ServerUrlTest, RejectsAUrlWithoutAHost) {
    EXPECT_EQ(checkServerUrl("https://").problem, ServerUrlProblem::Malformed);
    EXPECT_EQ(checkServerUrl("https:///pfad").problem, ServerUrlProblem::Malformed);
}

TEST(ServerUrlTest, RejectsAnUnterminatedIpv6Bracket) {
    // "[::1" ohne schliessende Klammer: hostOf() darf hier nicht raten, was
    // gemeint sein könnte - eine offene Klammer ist kein Rechnername.
    EXPECT_EQ(checkServerUrl("http://[::1").problem, ServerUrlProblem::Malformed);
}

TEST(ServerUrlTest, RejectsAHostThatMerelyEndsWithLocalhost) {
    // "evil.localhost" ist ein fremder Rechner - der Vergleich in
    // isLocalhost() ist exakt und darf nicht wie ein "endet mit" wirken.
    EXPECT_EQ(checkServerUrl("http://evil.localhost").problem,
              ServerUrlProblem::InsecureScheme);
}

TEST(ServerUrlTest, RejectsAnEmptyScheme) {
    // "://nas.example.com": das Schema vor "://" ist leer, keins von http
    // oder https - darf nicht stillschweigend als eines von beiden gelten.
    EXPECT_EQ(checkServerUrl("://nas.example.com").problem,
              ServerUrlProblem::UnsupportedScheme);
}

TEST(ServerUrlTest, RejectsLocalhostWithATrailingDot) {
    // "localhost." (mit Punkt) ist im DNS die FQDN-Schreibweise derselben
    // Adresse, aber isLocalhost() vergleicht exakt gegen "localhost" - der
    // Punkt macht daraus einen anderen, nicht als lokal erkannten String.
    // Absichtlich streng: eine Aufweichung dieses Vergleichs wäre der
    // einfachere Weg, versehentlich auch echte Fremdrechner durchzulassen.
    EXPECT_EQ(checkServerUrl("http://localhost.").problem,
              ServerUrlProblem::InsecureScheme);
}

TEST(ServerUrlTest, RejectsAHostHiddenBehindAClosedIpv6Bracket) {
    // "[::1]evil.com:8000": ohne die Prüfung auf das Zeichen direkt nach der
    // schliessenden Klammer würde hostOf() nur "[::1]" herauslesen und
    // "evil.com" stillschweigend verschlucken - localhost träfe zu, obwohl
    // die Anfrage tatsächlich an einen fremden Rechner ginge.
    EXPECT_EQ(checkServerUrl("http://[::1]evil.com:8000").problem,
              ServerUrlProblem::Malformed);
}

TEST(ServerUrlTest, ProducesAMessageForEveryProblem) {
    // Die Meldung landet später im Dialog. Kein Problem ohne Erklärung.
    EXPECT_FALSE(checkServerUrl("").message.empty());
    EXPECT_FALSE(checkServerUrl("https://").message.empty());
    EXPECT_FALSE(checkServerUrl("ftp://x").message.empty());
    EXPECT_FALSE(checkServerUrl("http://nas.example.com").message.empty());
    EXPECT_TRUE(checkServerUrl("https://nas.example.com").message.empty());
}
