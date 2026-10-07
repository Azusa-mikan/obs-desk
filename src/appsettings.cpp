#include "appsettings.h"

#include <QSettings>

namespace {
constexpr auto kHostKey = "connection/host";
constexpr auto kPortKey = "connection/port";
constexpr auto kPasswordKey = "connection/password";
constexpr auto kRememberPasswordKey = "connection/remember-password";

constexpr quint16 kDefaultPort = 4455;
} // namespace

QString AppSettings::host() {
    return QSettings().value(kHostKey, QStringLiteral("127.0.0.1")).toString();
}

void AppSettings::setHost(const QString &host) {
    QSettings().setValue(kHostKey, host);
}

quint16 AppSettings::port() {
    return static_cast<quint16>(QSettings().value(kPortKey, kDefaultPort).toUInt());
}

void AppSettings::setPort(quint16 port) {
    QSettings().setValue(kPortKey, port);
}

QString AppSettings::password() {
    if (!rememberPassword())
        return QString();
    return QSettings().value(kPasswordKey).toString();
}

void AppSettings::setPassword(const QString &password) {
    QSettings().setValue(kPasswordKey, password);
}

bool AppSettings::rememberPassword() {
    return QSettings().value(kRememberPasswordKey, false).toBool();
}

void AppSettings::setRememberPassword(bool remember) {
    QSettings().setValue(kRememberPasswordKey, remember);
}
