#include "core/api/ServerUrl.h"

#include <algorithm>
#include <cctype>

namespace baludesk::core {
namespace {

std::string trimmed(const std::string& text) {
    const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };

    auto begin = text.begin();
    while (begin != text.end() && isSpace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    auto end = text.end();
    while (end != begin && isSpace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    return std::string(begin, end);
}

std::string lowered(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

void stripTrailingSlashes(std::string& url) {
    while (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
}

// Der Teil zwischen Schema und erstem Schrägstrich, ohne Benutzerangabe und
// ohne Portnummer. Leer, wenn keiner da ist.
std::string hostOf(const std::string& urlWithoutScheme) {
    const std::size_t pathStart = urlWithoutScheme.find('/');
    std::string authority = urlWithoutScheme.substr(0, pathStart);

    // Eine Benutzerangabe vor dem @ gehört nicht zum Rechnernamen. Ohne
    // diesen Schnitt liesse sich "http://localhost@fremder.host" als
    // localhost ausgeben.
    const std::size_t at = authority.rfind('@');
    if (at != std::string::npos) {
        authority = authority.substr(at + 1);
    }

    if (!authority.empty() && authority.front() == '[') {
        // IPv6 in eckigen Klammern, etwa [::1]:8000
        const std::size_t close = authority.find(']');
        if (close == std::string::npos) {
            return {};
        }
        return authority.substr(0, close + 1);
    }

    const std::size_t colon = authority.find(':');
    if (colon != std::string::npos) {
        authority = authority.substr(0, colon);
    }
    return authority;
}

bool isLocalhost(const std::string& host) {
    const std::string lower = lowered(host);
    return lower == "localhost" || lower == "127.0.0.1" || lower == "[::1]";
}

}  // namespace

ServerUrlCheck checkServerUrl(const std::string& input) {
    ServerUrlCheck check;

    const std::string text = trimmed(input);
    if (text.empty()) {
        check.problem = ServerUrlProblem::Empty;
        check.message = "Bitte eine Serveradresse eingeben.";
        return check;
    }

    std::string url = text;
    std::string scheme;

    const std::size_t schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos) {
        // Ohne Angabe wird https angenommen. Andersherum - stillschweigend auf
        // http zurückfallen - wäre gefährlich.
        scheme = "https";
        url = "https://" + url;
    } else {
        scheme = lowered(url.substr(0, schemeEnd));
        if (scheme != "http" && scheme != "https") {
            check.problem = ServerUrlProblem::UnsupportedScheme;
            check.message = "Nur http und https werden unterstützt, nicht „" + scheme + "“.";
            return check;
        }
        url = scheme + url.substr(schemeEnd);
    }

    const std::string rest = url.substr(scheme.size() + 3);
    const std::string host = hostOf(rest);
    if (host.empty()) {
        check.problem = ServerUrlProblem::Malformed;
        check.message = "In der Adresse steckt kein Rechnername.";
        return check;
    }

    if (scheme == "http" && !isLocalhost(host)) {
        check.problem = ServerUrlProblem::InsecureScheme;
        check.message =
            "Unverschlüsseltes http ist nur zu localhost erlaubt. Über diese "
            "Verbindung geht ein Zugangstoken — bitte https verwenden.";
        return check;
    }

    stripTrailingSlashes(url);
    check.normalized = url;
    return check;
}

}  // namespace baludesk::core
