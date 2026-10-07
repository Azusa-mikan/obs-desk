#include "ui/mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    // Deterministic QSettings paths for the AppSettings helper.
    QCoreApplication::setOrganizationName(QStringLiteral("obs_desk"));
    QCoreApplication::setApplicationName(QStringLiteral("obs_desk"));
    QCoreApplication::setApplicationVersion(QStringLiteral(OBS_DESK_VERSION));

    MainWindow window;
    window.show();

    return app.exec();
}
