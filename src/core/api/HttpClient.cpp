#include "core/api/HttpClient.h"

#include <utility>

#include "core/util/Logger.h"

namespace baludesk::core {
namespace {

std::string withoutTrailingSlash(std::string url) {
    while (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
    return url;
}

}  // namespace

HttpClient::HttpClient(std::string baseUrl,
                       std::shared_ptr<Transport> transport,
                       TlsPolicy policy)
    : baseUrl_(withoutTrailingSlash(std::move(baseUrl))),
      transport_(std::move(transport)),
      policy_(std::move(policy)) {}

std::string HttpClient::urlFor(const std::string& path) const {
    if (path.empty()) {
        return baseUrl_;
    }
    if (path.front() == '/') {
        return baseUrl_ + path;
    }
    return baseUrl_ + "/" + path;
}

void HttpClient::setTlsPolicy(TlsPolicy policy) {
    const std::lock_guard<std::mutex> lock(policyMutex_);
    policy_ = std::move(policy);
}

HttpResult HttpClient::send(HttpRequest request) {
    const std::string url = urlFor(request.path);

    TlsPolicy policy;
    {
        const std::lock_guard<std::mutex> lock(policyMutex_);
        policy = policy_;
    }
    const TransportResult transportResult = transport_->send(url, request, policy);

    HttpResult result;
    result.error = transportResult.error;
    result.statusCode = transportResult.response.statusCode;
    result.body = transportResult.response.body;
    result.detail = transportResult.detail;
    result.peerCertificatePem = transportResult.peerCertificatePem;

    if (result.error != HttpErrorKind::None) {
        Logger::warn("HTTP: {} fehlgeschlagen: {}", url, result.detail);
    } else if (!result.ok()) {
        Logger::warn("HTTP: {} antwortete mit Status {}", url, result.statusCode);
    }

    return result;
}

HttpResult HttpClient::get(const std::string& path, const std::string& bearerToken) {
    HttpRequest request;
    request.method = HttpMethod::Get;
    request.path = path;
    request.bearerToken = bearerToken;
    return send(std::move(request));
}

HttpResult HttpClient::postJson(const std::string& path,
                                const std::string& jsonBody,
                                const std::string& bearerToken) {
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.path = path;
    request.jsonBody = jsonBody;
    request.bearerToken = bearerToken;
    return send(std::move(request));
}

}  // namespace baludesk::core
