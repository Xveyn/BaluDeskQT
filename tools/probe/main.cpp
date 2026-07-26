#include <iostream>
#include <memory>
#include <string>

#include "core/api/CurlTransport.h"
#include "core/api/HttpClient.h"
#include "core/util/Logger.h"
#include "core/util/TlsTrust.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Aufruf: baludesk_probe <server-url> [pin]\n";
        return 2;
    }

    baludesk::core::TlsPolicy policy;
    if (argc >= 3) {
        policy.mode = baludesk::core::TlsPolicy::Mode::Pinned;
        policy.publicKeyPin = argv[2];
    }

    auto transport = std::make_shared<baludesk::core::CurlTransport>();
    baludesk::core::HttpClient client(argv[1], transport, policy);

    const auto result = client.get("/api/health");

    std::cout << "ok:     " << (result.ok() ? "ja" : "nein") << "\n";
    std::cout << "status: " << result.statusCode << "\n";
    std::cout << "fehler: " << static_cast<int>(result.error) << "\n";
    std::cout << "detail: " << result.detail << "\n";
    std::cout << "rumpf:  " << result.body << "\n";

    if (!result.peerCertificatePem.empty()) {
        const auto identity = baludesk::core::identifyCertificate(result.peerCertificatePem);
        if (identity) {
            std::cout << "fingerabdruck: " << identity->fingerprintSha256 << "\n";
            std::cout << "pin:           " << identity->publicKeyPin << "\n";
        } else {
            std::cout << "Zertifikat der Gegenstelle nicht lesbar\n";
        }
    }
    return result.ok() ? 0 : 1;
}
