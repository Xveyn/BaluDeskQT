#include "bridge/AuthBridge.h"

#include <QTimer>

#include <chrono>
#include <cstdint>
#include <utility>

#include "core/api/HttpClient.h"
#include "core/api/ServerUrl.h"
#include "core/api/TlsPolicy.h"
#include "core/auth/FileSessionRepository.h"
#include "core/util/Logger.h"

namespace baludesk::bridge {

namespace {

std::int64_t nowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

}  // namespace

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

    // Mit this als Eltern: der Zeitgeber wandert bei einem moveToThread mit
    // und gehört danach demselben Thread wie alles Übrige.
    pollTimer_ = new QTimer(this);
    pollTimer_->setSingleShot(true);
    connect(pollTimer_, &QTimer::timeout, this, &AuthBridge::pollNow);
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
            emit serverUnreachable(serverUrl,
                                   QString::fromStdString(session->username),
                                   tr("Der Server antwortet unerwartet (Status %1).")
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
            emit serverUnreachable(serverUrl,
                                   QString::fromStdString(session->username),
                                   tr("Der Server ist zurzeit nicht erreichbar."));
            return;
    }
}

void AuthBridge::logout() {
    const bool complete = repository_->clear();

    // ABBAUREIHENFOLGE IST TRAGEND: TrustNegotiation und AuthenticatedClient
    // halten je eine HttpClient& und sagen in ihren Kopfdateien zu, dass er
    // sie überlebt. resetPairing() räumt trust_ und pairing_ ab, also gehört
    // es vor die beiden reset()-Aufrufe - erreichbar ist der Fall über den
    // Zweig PinMismatch in connectToServer(), nach dem trust_ und http_ noch
    // leben, wenn der Nutzer „Abmelden und neu verbinden" wählt.
    resetPairing();
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

void AuthBridge::resetPairing() {
    pollTimer_->stop();
    pairing_.reset();
    trust_.reset();
    pendingServerUrl_.clear();
}

void AuthBridge::connectToServer(const QString& rawUrl) {
    resetPairing();

    // Die Regel gilt für jeden Aufrufer, nicht nur für den Dialog - deshalb
    // liegt sie in core und wird hier nur benutzt.
    const auto check = core::checkServerUrl(rawUrl.toStdString());
    if (!check.ok()) {
        emit serverUrlRejected(QString::fromStdString(check.message));
        return;
    }

    pendingServerUrl_ = QString::fromStdString(check.normalized);

    // Wie in logout(): der bisherige AuthenticatedClient hält eine Referenz
    // auf den alten HttpClient und muss weg, bevor dieser ersetzt wird.
    // Erreichbar über restoreSession() mit ReauthRequired - das lässt die
    // Sitzung stehen, und der Nutzer gibt daraufhin eine Adresse ein.
    authenticated_.reset();

    // Frische Verbindung mit voller Prüfung. Ein etwa früher angehefteter
    // Schlüssel gehört zu einer anderen Bindung und hätte hier nichts zu
    // suchen.
    http_ = std::make_unique<core::HttpClient>(check.normalized, transport_, core::TlsPolicy{});
    trust_ = std::make_unique<core::TrustNegotiation>(*http_);

    switch (trust_->probe()) {
        case core::TrustState::Trusted:
            beginPairing();
            return;

        case core::TrustState::ConfirmationRequired:
            emit certificateConfirmationRequired(
                QString::fromStdString(trust_->prompt()->fingerprintSha256),
                QString::fromStdString(trust_->prompt()->reason));
            return;

        case core::TrustState::PinMismatch:
            emit certificateChanged(pendingServerUrl_,
                                    QString::fromStdString(trust_->message()));
            // Erst melden, dann abräumen: die Meldung liest pendingServerUrl_
            // und trust_. Ohne das Abräumen bliebe ein Vorgang stehen, den
            // niemand mehr fortsetzen kann - und ein verspäteter Klick auf
            // „Abbrechen" fände ihn vor.
            resetPairing();
            return;

        case core::TrustState::Unreachable:
        case core::TrustState::Failed:
        case core::TrustState::Idle:
            emit pairingFailed(QString::fromStdString(trust_->message()));
            resetPairing();
            return;
    }
}

void AuthBridge::confirmCertificate() {
    if (!trust_) {
        return;
    }

    if (trust_->confirm() == core::TrustState::Trusted) {
        beginPairing();
        return;
    }

    emit pairingFailed(QString::fromStdString(trust_->message()));
    resetPairing();
}

void AuthBridge::rejectCertificate() {
    // Derselbe Wächter wie in pollNow(), aus demselben Grund: die Oberfläche
    // stellt ihre Aufrufe in die Warteschlange dieses Threads, während hier
    // noch ein blockierender Aufruf läuft. Ohne den Wächter meldete ein
    // längst überholter Klick pairingCancelled - und die Oberfläche wechselt
    // darauf bedingungslos zur Eingabeseite zurück.
    if (!trust_) {
        return;
    }

    resetPairing();
    emit pairingCancelled();
}

void AuthBridge::cancelPairing() {
    // Siehe rejectCertificate(): ein Abbruch, der eine bereits abgelegte
    // Sitzung von der Anzeige risse, wäre der teuerste Fall - die Token
    // liegen dann auf der Platte, und die Anwendung behauptet das Gegenteil.
    if (!pairing_) {
        return;
    }

    resetPairing();
    emit pairingCancelled();
}

void AuthBridge::beginPairing() {
    pairing_ = std::make_unique<core::PairingClient>(*http_);

    const auto progress = pairing_->begin(deviceId_.toStdString(),
                                          deviceName_.toStdString(),
                                          nowSeconds());

    if (progress.state != core::PairingState::WaitingForApproval) {
        emit pairingFailed(QString::fromStdString(progress.message));
        resetPairing();
        return;
    }

    emit pairingCodeReady(QString::fromStdString(pairing_->prompt()->userCode),
                          QString::fromStdString(pairing_->prompt()->verificationUrl),
                          pairing_->prompt()->expiresInSeconds);

    // Das vom Server vorgegebene Intervall ist einzuhalten und nicht zu
    // unterbieten: bei 12 zugelassenen Abrufen je Minute und 5 Sekunden
    // Vorgabe gibt es keine Reserve.
    pollTimer_->start(progress.waitSeconds * 1000);
}

void AuthBridge::pollNow() {
    if (!pairing_) {
        return;
    }

    const auto progress = pairing_->poll(nowSeconds());

    if (progress.state == core::PairingState::WaitingForApproval) {
        if (!progress.message.empty()) {
            emit pairingNotice(QString::fromStdString(progress.message));
        }
        pollTimer_->start(progress.waitSeconds * 1000);
        return;
    }

    if (progress.state == core::PairingState::Approved) {
        finishPairing();
        return;
    }

    emit pairingFailed(QString::fromStdString(progress.message));
    resetPairing();
}

void AuthBridge::finishPairing() {
    // Zwischen Approved und save() darf nichts stehen, was scheitern kann:
    // der Server hat seinen Datensatz beim Ausliefern gelöscht und antwortet
    // danach mit 404. Was hier verlorengeht, ist endgültig verloren.
    core::Session session;
    session.serverUrl = pendingServerUrl_.toStdString();
    session.username = pairing_->tokens()->username;
    session.publicKeyPin = trust_->confirmedPin();
    session.accessToken = pairing_->tokens()->accessToken;
    session.refreshToken = pairing_->tokens()->refreshToken;

    if (!repository_->save(session)) {
        core::Logger::error("Paarung: Sitzung konnte nicht abgelegt werden");
        emit pairingFailed(
            tr("Die Zugangsdaten konnten nicht abgelegt werden. Der Server liefert sie "
               "kein zweites Mal — bitte erneut verbinden."));
        resetPairing();
        return;
    }

    const QString username = QString::fromStdString(session.username);
    const QString serverUrl = pendingServerUrl_;

    authenticated_ = std::make_unique<core::AuthenticatedClient>(*http_, *repository_, session);

    core::Logger::info("Paarung: abgeschlossen für Benutzer {}", session.username);
    resetPairing();
    emit sessionEstablished(serverUrl, username);
}

}  // namespace baludesk::bridge
