#ifdef _WIN32
#include <windows.h>
#endif

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <ios>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "core/api/CurlTransport.h"
#include "core/api/HttpClient.h"
#include "core/api/ServerUrl.h"
#include "core/api/TlsPolicy.h"
#include "core/api/TrustNegotiation.h"
#include "core/auth/FileSessionRepository.h"
#include "core/auth/PairingClient.h"
#include "core/auth/SessionPaths.h"
#include "core/util/Logger.h"
#include "core/util/WindowsCredentialStore.h"

namespace {

using namespace baludesk::core;

std::int64_t nowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// Einzige Quelle der Wahrheit für den Ablageort - siehe
// core/auth/SessionPaths.h. Ein eigener, lokaler Aufbau dieses Pfads (wie
// früher hier) hält nur zufällig mit dem der Oberfläche Schritt.
std::string sessionPath() {
    return defaultSessionPath();
}

void printUsage() {
    std::cerr << "Aufruf:\n"
              << "  baludesk_probe pair <server-url>   Paarung durchführen\n"
              << "  baludesk_probe status              abgelegte Sitzung prüfen\n"
              << "  baludesk_probe logout              Sitzung entfernen\n";
}

// Baut eine TLS-Regel. Der Ablauf selbst liegt in core/api/TrustNegotiation -
// hier bleibt nur, was Kommandozeile ist: fragen und ausgeben. Genau diese
// Trennung erlaubt es der Oberfläche in Stufe 1c, denselben Ablauf mit einem
// Dialog zu fahren, statt ihn nachzubauen.
bool establishTrust(HttpClient& http, std::string& pinOut) {
    TrustNegotiation trust(http);

    TrustState state = trust.probe();

    if (state == TrustState::ConfirmationRequired) {
        std::cout << "\nDem Zertifikat dieses Servers wird nicht vertraut.\n"
                  << "Grund:         " << trust.prompt()->reason << "\n"
                  << "Fingerabdruck: " << trust.prompt()->fingerprintSha256 << "\n\n"
                  << "Vergleiche ihn mit dem Server. Vertrauen? [j/N] ";

        std::string answer;
        std::getline(std::cin, answer);
        if (answer != "j" && answer != "J") {
            std::cerr << "Abgebrochen.\n";
            return false;
        }

        state = trust.confirm();
    }

    if (state != TrustState::Trusted) {
        std::cerr << trust.message() << "\n";
        return false;
    }

    pinOut = trust.confirmedPin();
    if (pinOut.empty()) {
        std::cout << "Zertifikat regulär geprüft, kein Anheften nötig.\n";
    } else {
        std::cout << "Zertifikat bestätigt und angeheftet.\n";
    }
    return true;
}

int commandPair(const std::string& rawUrl) {
    const auto check = checkServerUrl(rawUrl);
    if (!check.ok()) {
        std::cerr << check.message << "\n";
        return 2;
    }
    std::cout << "Server: " << check.normalized << "\n";

    auto transport = std::make_shared<CurlTransport>();
    HttpClient http(check.normalized, transport, TlsPolicy{});

    std::string pin;
    if (!establishTrust(http, pin)) {
        return 1;
    }

    PairingClient pairing(http);
    auto progress = pairing.begin("baludesk-probe", "BaluDesk Pruefprogramm", nowSeconds());

    if (progress.state != PairingState::WaitingForApproval) {
        std::cerr << "Paarung nicht gestartet: " << progress.message << "\n";
        return 1;
    }

    std::cout << "\n=========================================\n"
              << "  Code:    " << pairing.prompt()->userCode << "\n"
              << "  Adresse: " << pairing.prompt()->verificationUrl << "\n"
              << "=========================================\n\n"
              << "Im Browser öffnen, anmelden, Code eingeben und freigeben.\n"
              << "Warte…\n";

    while (progress.state == PairingState::WaitingForApproval) {
        std::this_thread::sleep_for(std::chrono::seconds(progress.waitSeconds));
        progress = pairing.poll(nowSeconds());
        if (!progress.message.empty()) {
            std::cout << progress.message << "\n";
        }
    }

    if (progress.state != PairingState::Approved) {
        std::cerr << "Paarung nicht abgeschlossen.\n";
        return 1;
    }

    // Sofort ablegen. Der Server hat seinen Datensatz beim Ausliefern
    // gelöscht - was hier verloren geht, ist endgültig verloren.
    Session session;
    session.serverUrl = check.normalized;
    session.username = pairing.tokens()->username;
    session.publicKeyPin = pin;
    session.accessToken = pairing.tokens()->accessToken;
    session.refreshToken = pairing.tokens()->refreshToken;

    WindowsCredentialStore secrets;
    FileSessionRepository repository(sessionPath(), secrets);
    if (!repository.save(session)) {
        std::cerr << "Die Sitzung konnte nicht abgelegt werden. "
                     "Die Token sind damit verloren, bitte neu paaren.\n";
        return 1;
    }

    std::cout << "\nGepaart. Verbunden mit " << session.serverUrl
              << " als " << session.username << "\n";
    return 0;
}

int commandStatus() {
    WindowsCredentialStore secrets;
    FileSessionRepository repository(sessionPath(), secrets);

    const auto session = repository.load();
    if (!session) {
        std::cout << "Keine Sitzung abgelegt.\n";
        return 1;
    }

    std::cout << "Verbunden mit " << session->serverUrl << " als " << session->username << "\n"
              << "Zertifikat:   "
              << (session->publicKeyPin.empty() ? "regulär geprüft" : "angeheftet") << "\n";

    auto transport = std::make_shared<CurlTransport>();
    HttpClient http(session->serverUrl, transport,
                    TlsPolicy::pinnedFromConfirmedCertificate(session->publicKeyPin));

    const HttpResult result = http.get("/api/auth/me", session->accessToken);
    std::cout << "\n/api/auth/me → Status " << result.statusCode
              << (result.ok() ? " (gültig)" : " (nicht gültig)") << "\n";
    if (!result.body.empty()) {
        std::cout << result.body << "\n";
    }
    if (!result.detail.empty()) {
        std::cout << "Detail: " << result.detail << "\n";
    }

    return result.ok() ? 0 : 1;
}

int commandLogout() {
    WindowsCredentialStore secrets;
    FileSessionRepository repository(sessionPath(), secrets);

    if (!repository.clear()) {
        std::cerr << "Sitzung nur unvollständig entfernt. Im "
                     "Anmeldeinformationsspeicher unter „BaluDesk“ nachsehen.\n";
        return 1;
    }

    std::cout << "Sitzung entfernt.\n";
    return 0;
}

int run(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 2;
    }

    const std::string command = argv[1];

    if (command == "pair") {
        if (argc < 3) {
            printUsage();
            return 2;
        }
        return commandPair(argv[2]);
    }
    if (command == "status") {
        return commandStatus();
    }
    if (command == "logout") {
        return commandLogout();
    }

    printUsage();
    return 2;
}

}  // namespace

int main(int argc, char* argv[]) {
#ifdef _WIN32
    // Die Konsole steht sonst auf der lokalen Windows-Codepage - deutscher
    // Text (etwa "Öffnen") kommt dort als "Ãffnen" an. Die Daten selbst sind
    // korrekt UTF-8, nur die Konsole liest sie falsch.
    SetConsoleOutputCP(CP_UTF8);
#endif
    // std::cout ist bei Umleitung (Datei, Pipe) vollständig gepuffert statt
    // zeilengepuffert - der sechsstellige Paarungscode käme in einem
    // automatisierten Lauf sonst erst beim Programmende an, wenn überhaupt.
    // Genau das hat einen automatisierten Abnahmelauf schon einmal blind
    // gemacht.
    std::cout << std::unitbuf;

    // Protokollverzeichnis neben der Sitzungsdatei, nicht in ihr - beide
    // leiten sich aus demselben, einzig verbindlichen Pfad ab
    // (core/auth/SessionPaths.h). Ohne dieses Protokoll ist nicht
    // erkennbar, welches Token fehlschlug, ob der Rückbau selbst
    // fehlschlug oder warum ein Abruf erneut versucht wurde - alles Fragen,
    // die die Bibliothek nur über den Logger beantwortet.
    const std::filesystem::path logDirectory =
        std::filesystem::path(defaultSessionPath()).parent_path() / "logs";
    Logger::init(logDirectory.string(), 1024 * 1024, 3);

    const int result = run(argc, argv);

    Logger::shutdown();
    return result;
}
