#pragma once

#include <QString>

/// Persisted connection details (`QSettings`).
///
/// The MVP keeps only the *last* used connection. A saved-connection list
/// belongs in a later iteration; the storage keys are already namespaced
/// (`connection/...`) so adding an array later does not collide.
///
/// The password is only stored when the user opts in ("Remember password"
/// on the connect page) and it is stored in plain text - `QSettings` has no
/// secret store behind it on Linux. That is called out in the README.
class AppSettings {
public:
    static QString host();
    static void setHost(const QString &host);

    static quint16 port();
    static void setPort(quint16 port);

    static QString password();
    static void setPassword(const QString &password);

    static bool rememberPassword();
    static void setRememberPassword(bool remember);
};
