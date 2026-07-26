#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "core/api/CurlTransport.h"
#include "core/api/HttpClient.h"
#include "core/api/ServerUrl.h"
#include "core/api/TlsPolicy.h"
#include "core/auth/FileSessionRepository.h"
#include "core/auth/PairingClient.h"
#include "core/util/Logger.h"
#include "core/util/TlsTrust.h"
#include "core/util/WindowsCredentialStore.h"

namespace {

using namespace baludesk::core;

std::int64_t nowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::string sessionPath() {
    // std::getenv gilt bei MSVC unter /W4 als "unsicher" (C4996) und würde die
    // Warnungsfreiheit des Builds verletzen. Hier bewusst und begrenzt
    // unterdrückt statt projektweit, da nur diese eine Stelle betroffen ist.
#pragma warning(push)
#pragma warning(disable : 4996)
    const char* localAppData = std::getenv("LOCALAPPDATA");
#pragma warning(pop)
    const std::string base = localAppData != nullptr ? localAppData : ".";
    return base + "/BaluHost/BaluDesk/session.json";
}

void printUsage() {
    std::cerr << "Aufruf:\n"
              << "  baludesk_probe pair <server-url>   Paarung durchführen\n"
              << "  baludesk_probe status              abgelegte Sitzung prüfen\n"
              << "  baludesk_probe logout              Sitzung entfernen\n";
}

// Baut eine TLS-Regel. Bei unbekanntem Zertifikat wird der Fingerabdruck
// angezeigt und auf der Kommandozeile bestätigt - genau das, was in Stufe 1c
// ein Dialog tut.
bool establishTrust(HttpClient& http, std::string& pinOut) {
    HttpResult result = http.get("/api/health");

    if (result.error == HttpErrorKind::CertificatePinMismatch) {
        std::cerr << "Das Zertifikat weicht vom bestätigten ab. Abbruch.\n";
        return false;
    }

    if (result.error == HttpErrorKind::CertificateUntrusted) {
        if (result.peerCertificatePem.empty()) {
            std::cerr << "Zertifikat nicht vertrauenswürdig, und es war keines auszulesen.\n";
            return false;
        }
        const auto identity = identifyCertificate(result.peerCertificatePem);
        if (!identity) {
            std::cerr << "Zertifikat der Gegenstelle ist nicht lesbar.\n";
            return false;
        }

        std::cout << "\nDem Zertifikat dieses Servers wird nicht vertraut.\n"
                  << "Grund:         " << result.detail << "\n"
                  << "Fingerabdruck: " << identity->fingerprintSha256 << "\n\n"
                  << "Vergleiche ihn mit dem Server. Vertrauen? [j/N] ";

        std::string answer;
        std::getline(std::cin, answer);
        if (answer != "j" && answer != "J") {
            std::cerr << "Abgebrochen.\n";
            return false;
        }

        pinOut = identity->publicKeyPin;
        http.setTlsPolicy(TlsPolicy::pinnedFromConfirmedCertificate(pinOut));

        result = http.get("/api/health");
        if (!result.ok()) {
            std::cerr << "Auch mit angeheftetem Schlüssel keine Verbindung: "
                      << result.detail << "\n";
            return false;
        }
        std::cout << "Zertifikat bestätigt und angeheftet.\n";
        return true;
    }

    if (!result.ok()) {
        std::cerr << "Server nicht erreichbar: " << result.detail
                  << " (Status " << result.statusCode << ")\n";
        return false;
    }

    std::cout << "Zertifikat regulär geprüft, kein Anheften nötig.\n";
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
    repository.clear();
    std::cout << "Sitzung entfernt.\n";
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
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
