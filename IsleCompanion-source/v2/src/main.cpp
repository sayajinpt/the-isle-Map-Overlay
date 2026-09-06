#include "app/Application.h"

#include "core/AppIcon.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication qt(argc, argv);
    QApplication::setApplicationName(QStringLiteral("The Isle Companion v2"));
    QApplication::setOrganizationName(QStringLiteral("IsleCompanion"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QApplication::setQuitOnLastWindowClosed(true);

    const QIcon icon = isle::makeAppIcon();
    qt.setWindowIcon(icon);

    isle::Application app;
    if (app.run() != 0) {
        return 0;
    }
    return qt.exec();
}
