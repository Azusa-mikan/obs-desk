#pragma once

#include <QAbstractSocket>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>

class QTimer;
class QWebSocket;

/// Low-level obs-websocket v5 client.
///
/// Owns the socket and speaks the JSON envelope protocol:
///
///   Hello (op=0) -> Identify (op=1) -> Identified (op=2)
///
/// After `identified()` is emitted the client can send requests and receive
/// responses/events. Responses are dispatched by `requestId` (a map of
/// outstanding ids is kept so a request can be correlated with its reply).
///
/// Error reporting is centralised in `failConnection()`: it emits
/// `connectionFailed()` at most once per connection attempt and suppresses the
/// `disconnected()` fallback so callers never see a failure followed by a
/// spurious clean close.
class ObsClient : public QObject {
    Q_OBJECT

public:
    explicit ObsClient(QObject *parent = nullptr);
    ~ObsClient() override;

    /// Opens `ws://host:port` and starts the handshake. `password` may be
    /// empty when OBS has no authentication configured.
    void connectToObs(const QString &host, quint16 port, const QString &password);

    /// Closes the socket and resets the handshake state.
    void disconnectFromObs();

    /// True once the Identify/Identified exchange has completed.
    bool isIdentified() const;

    /// Sends a request and returns its generated `requestId` (UUID without
    /// braces). Returns an empty-able id even when not connected; the request
    /// is only put on the wire when the socket is connected.
    QString sendRequest(const QString &requestType, const QJsonObject &requestData = {});

signals:
    /// TCP/WebSocket layer is up, handshake has not completed yet.
    void socketConnected();
    /// Handshake finished (OBS sent op=2 Identified).
    void identified();
    /// Socket closed cleanly (or by the user), no error.
    void connectionClosed();
    /// Human-readable reason for a failed connect/timeout/auth/handshake.
    void connectionFailed(const QString &reason);
    /// A request completed successfully.
    void responseReceived(const QString &requestType, const QJsonObject &responseData);
    /// A request completed with `requestStatus.result == false`.
    void requestFailed(const QString &requestType, int code, const QString &comment);
    /// An obs-websocket event (op=5) arrived.
    void eventReceived(const QString &eventType, const QJsonObject &eventData);

private slots:
    void onConnected();
    void onTextMessageReceived(const QString &message);
    void onDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onHandshakeTimeout();

private:
    void sendIdentify(const QJsonObject &helloData);
    void handleRequestResponse(const QJsonObject &d);
    void handleEvent(const QJsonObject &d);

    /// authString = base64( sha256( base64( sha256( password + salt ) ) +
    ///                              challenge ) ). See comment in the .cpp.
    static QByteArray buildAuthString(const QString &password, const QString &salt, const QString &challenge);

    void failConnection(const QString &reason);

    QWebSocket *m_socket = nullptr;
    QTimer *m_handshakeTimer = nullptr;
    QHash<QString, QString> m_pendingRequests; // requestId -> requestType
    QString m_password;
    bool m_identified = false;
    bool m_suppressClosed = false; // set by failConnection() to mute onDisconnected
};
