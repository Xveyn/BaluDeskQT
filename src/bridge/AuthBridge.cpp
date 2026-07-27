#include "bridge/AuthBridge.h"

#include <utility>

#include "core/api/HttpClient.h"
#include "core/api/TlsPolicy.h"
#include "core/auth/FileSessionRepository.h"
#include "core/util/Logger.h"

namespace baludesk::bridge {

AuthBridge::AuthBridge(std::shared_ptr<core::Transport> transport,
                       std::unique_ptr<core::SecretStore> secrets,
                       std::string sessionPath,
                       QString deviceId,
                       QString deviceName,
                       QObject* parent)
    : QObject(parent),
      transport_(std::move(transport)),
      secrets_(std::move(secrets)),
      deviceId_(std::move(deviceId)),
      deviceName_(std::move(deviceName)) {
    repository_ =
        std::make_unique<core::FileSessionRepository>(std::move(sessionPath), *secrets_);
}

AuthBridge::~AuthBridge() = default;

void AuthBridge::restoreSession() {
    const auto session = repository_->load();
    if (!session) {
        core::Logger::info("Anmeldung: keine Sitzung abgelegt");
        emit noSession();
        return;
    }

    const QString serverUrl = QString::fromStdString(session->serverUrl);

    http_ = std::make_unique<core::HttpClient>(
        session->serverUrl,
        transport_,
        core::TlsPolicy::pinnedFromConfirmedCertificate(session->publicKeyPin));

    authenticated_ =
        std::make_unique<core::AuthenticatedClient>(*http_, *repository_, *session);

    const auto result = authenticated_->get("/api/auth/me");

    switch (result.outcome) {
        case core::AuthOutcome::Ok:
        case core::AuthOutcome::Refreshed:
            if (result.response.ok()) {
                core::Logger::info("Anmeldung: Sitzung wiederhergestellt für {}",
                                   session->username);
                emit sessionEstablished(serverUrl, QString::fromStdString(session->username));
                return;
            }
            if (result.response.statusCode == 401) {
                // Erneuert und trotzdem abgewiesen: die Bindung ist beim
                // Server nicht mehr gültig.
                emit reauthRequired(tr("Die Anmeldung gilt nicht mehr. Bitte neu verbinden."));
                return;
            }
            emit serverUnreachable(tr("Der Server antwortet unerwartet (Status %1).")
                                       .arg(result.response.statusCode));
            return;

        case core::AuthOutcome::ReauthRequired:
            emit reauthRequired(tr("Die Sitzung ist abgelaufen. Bitte neu verbinden."));
            return;

        case core::AuthOutcome::TrustBroken:
            emit certificateChanged(serverUrl,
                                    QString::fromStdString(result.response.detail));
            return;

        case core::AuthOutcome::Unreachable:
            emit serverUnreachable(tr("Der Server ist zurzeit nicht erreichbar."));
            return;
    }
}

void AuthBridge::logout() {
    const bool complete = repository_->clear();

    authenticated_.reset();
    http_.reset();

    if (complete) {
        emit loggedOut();
        return;
    }

    emit logoutIncomplete(
        tr("Die Sitzung wurde entfernt, aber nicht alles liess sich löschen. "
           "Bitte im Anmeldeinformationsspeicher unter „BaluDesk“ nachsehen."));
}

}  // namespace baludesk::bridge
