#include "protocol/obsclient.h"

#include "protocol/obsprotocol.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLoggingCategory>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QWebSocket>

namespace {
Q_LOGGING_CATEGORY(obsClientLog, "obs_control.client")

/// Time allowed for the TCP/WebSocket connect, and separately for the
/// Hello -> Identify -> Identified exchange after the socket opens.
constexpr int kHandshakeTimeoutMs = 5000;
} // namespace

ObsClient::ObsClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
    , m_handshakeTimer(new QTimer(this)) {
    m_handshakeTimer->setSingleShot(true);
    m_handshakeTimer->setInterval(kHandshakeTimeoutMs);

    connect(m_socket, &QWebSocket::connected, this, &ObsClient::onConnected);
    connect(m_socket, &QWebSocket::textMessageReceived, this, &ObsClient::onTextMessageReceived);
    connect(m_socket, &QWebSocket::disconnected, this, &ObsClient::onDisconnected);
    connect(m_socket, &QWebSocket::errorOccurred, this, &ObsClient::onSocketError);
    connect(m_handshakeTimer, &QTimer::timeout, this, &ObsClient::onHandshakeTimeout);
}

ObsClient::~ObsClient() = default;

void ObsClient::connectToObs(const QString &host, quint16 port, const QString &password) {
    m_password = password;
    m_identified = false;
    m_suppressClosed = false;
    m_pendingRequests.clear();

    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->abort();

    QUrl url;
    url.setScheme(QStringLiteral("ws"));
    url.setHost(host.isEmpty() ? QStringLiteral("127.0.0.1") : host);
    url.setPort(port);
    qCInfo(obsClientLog) << "connecting to" << url.toString();
    m_socket->open(url);

    // Guard the connect phase too: an unreachable host can otherwise hang until
    // the OS TCP timeout. onConnected() restarts the timer for the handshake.
    m_handshakeTimer->start();
}

void ObsClient::disconnectFromObs() {
    m_handshakeTimer->stop();
    m_identified = false;
    m_pendingRequests.clear();
    // A user-initiated close is a clean close, not a failure.
    m_suppressClosed = false;
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->close();
}

bool ObsClient::isIdentified() const {
    return m_identified;
}

QString ObsClient::sendRequest(const QString &requestType, const QJsonObject &requestData) {
    const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        qCWarning(obsClientLog) << "dropping request" << requestType << "- socket not connected";
        return requestId;
    }

    m_pendingRequests.insert(requestId, requestType);

    QJsonObject d;
    d[QStringLiteral("requestType")] = requestType;
    d[QStringLiteral("requestId")] = requestId;
    if (!requestData.isEmpty())
        d[QStringLiteral("requestData")] = requestData;

    QJsonObject envelope;
    envelope[QStringLiteral("op")] = obs::op::Request;
    envelope[QStringLiteral("d")] = d;

    m_socket->sendTextMessage(QString::fromUtf8(QJsonDocument(envelope).toJson(QJsonDocument::Compact)));
    return requestId;
}

void ObsClient::onConnected() {
    qCInfo(obsClientLog) << "socket connected, waiting for Hello";
    m_handshakeTimer->start();
    emit socketConnected();
}

void ObsClient::onTextMessageReceived(const QString &message) {
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(obsClientLog) << "ignoring malformed message:" << parseError.errorString();
        return;
    }

    const QJsonObject envelope = doc.object();
    const int op = envelope.value(QStringLiteral("op")).toInt(-1);
    const QJsonObject d = envelope.value(QStringLiteral("d")).toObject();

    switch (op) {
    case obs::op::Hello:
        sendIdentify(d);
        break;
    case obs::op::Identified:
        m_handshakeTimer->stop();
        m_identified = true;
        qCInfo(obsClientLog) << "identified";
        emit identified();
        break;
    case obs::op::Event:
        handleEvent(d);
        break;
    case obs::op::RequestResponse:
        handleRequestResponse(d);
        break;
    case obs::op::RequestBatchResponse: {
        // A batch reply carries an array of ordinary response objects.
        const QJsonArray results = d.value(QStringLiteral("results")).toArray();
        for (const QJsonValue &result : results)
            handleRequestResponse(result.toObject());
        break;
    }
    default:
        qCWarning(obsClientLog) << "ignoring unknown op" << op;
        break;
    }
}

void ObsClient::onDisconnected() {
    m_handshakeTimer->stop();
    m_identified = false;
    m_pendingRequests.clear();

    // obs-websocket closes with codes in the 4000 range for protocol problems.
    const int code = static_cast<int>(m_socket->closeCode());
    QString failure;
    switch (code) {
    case 4009:
        failure = tr("Authentication failed \u2014 check the password");
        break;
    case 4007:
        failure = tr("Handshake error: OBS did not receive an Identify message");
        break;
    case 4008:
        // "Already identified" - an informational close, treat as clean.
        break;
    default:
        if (code >= 4000 && code <= 4999)
            failure = tr("Connection closed by OBS (code %1)").arg(code);
        break;
    }

    if (!m_suppressClosed) {
        if (!failure.isEmpty())
            emit connectionFailed(failure);
        else
            emit connectionClosed();
    }
    m_suppressClosed = false;
}

void ObsClient::onSocketError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error)
    if (m_identified) {
        // A drop during an active session; let disconnected() report it.
        return;
    }
    failConnection(tr("Could not connect: %1").arg(m_socket->errorString()));
}

void ObsClient::onHandshakeTimeout() {
    if (m_identified)
        return;
    // Still connecting means the host/port was unreachable; a connected socket
    // that has not identified means the Hello/Identify exchange stalled.
    if (m_socket->state() == QAbstractSocket::ConnectedState)
        failConnection(tr("Handshake timed out"));
    else
        failConnection(tr("Connection timed out"));
}

void ObsClient::sendIdentify(const QJsonObject &helloData) {
    QJsonObject d;
    d[QStringLiteral("rpcVersion")] = 1;
    d[QStringLiteral("eventSubscriptions")] = obs::eventSub::All;

    // `authentication` is only present when OBS has a password configured.
    const QJsonValue authValue = helloData.value(QStringLiteral("authentication"));
    if (authValue.isObject()) {
        const QJsonObject auth = authValue.toObject();
        const QString salt = auth.value(QStringLiteral("salt")).toString();
        const QString challenge = auth.value(QStringLiteral("challenge")).toString();
        d[QStringLiteral("authentication")] =
            QString::fromUtf8(buildAuthString(m_password, salt, challenge));
    }

    QJsonObject envelope;
    envelope[QStringLiteral("op")] = obs::op::Identify;
    envelope[QStringLiteral("d")] = d;
    m_socket->sendTextMessage(QString::fromUtf8(QJsonDocument(envelope).toJson(QJsonDocument::Compact)));
}

void ObsClient::handleRequestResponse(const QJsonObject &d) {
    const QString requestType = d.value(QStringLiteral("requestType")).toString();
    const QString requestId = d.value(QStringLiteral("requestId")).toString();
    const QJsonObject status = d.value(QStringLiteral("requestStatus")).toObject();
    const QJsonObject responseData = d.value(QStringLiteral("responseData")).toObject();

    m_pendingRequests.remove(requestId);

    const bool result = status.value(QStringLiteral("result")).toBool(false);
    if (result) {
        emit responseReceived(requestType, responseData);
    } else {
        emit requestFailed(requestType,
                           status.value(QStringLiteral("code")).toInt(),
                           status.value(QStringLiteral("comment")).toString());
    }
}

void ObsClient::handleEvent(const QJsonObject &d) {
    const QString eventType = d.value(QStringLiteral("eventType")).toString();
    const QJsonObject eventData = d.value(QStringLiteral("eventData")).toObject();
    if (eventType.isEmpty())
        return;
    emit eventReceived(eventType, eventData);
}

void ObsClient::failConnection(const QString &reason) {
    if (m_suppressClosed)
        return;
    m_suppressClosed = true;
    m_handshakeTimer->stop();
    m_identified = false;
    emit connectionFailed(reason);
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->abort();
}

QByteArray ObsClient::buildAuthString(const QString &password, const QString &salt, const QString &challenge) {
    // obs-websocket v5 authentication:
    //   secret       = sha256( password + salt )          -> raw 32 bytes
    //   secretBase64 = base64( secret )                   -> ASCII
    //   auth         = base64( sha256( secretBase64 + challenge ) )
    // The concatenations are performed on UTF-8 bytes (the salt/challenge are
    // ASCII in practice, but toUtf8 keeps this correct for any password).
    const QByteArray secret =
        QCryptographicHash::hash((password + salt).toUtf8(), QCryptographicHash::Sha256).toBase64();
    return QCryptographicHash::hash(secret + challenge.toUtf8(), QCryptographicHash::Sha256)
        .toBase64();
}
