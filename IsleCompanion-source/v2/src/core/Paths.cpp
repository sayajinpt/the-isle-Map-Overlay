#include "core/Paths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace isle::paths {
namespace {

QString resolveAssetRoot()
{
#ifdef ISLE_ASSET_ROOT
    const QString compiled(QString::fromUtf8(ISLE_ASSET_ROOT));
    if (QDir(compiled).exists()) {
        return QDir(compiled).absolutePath();
    }
#endif
    const QDir appDir(QCoreApplication::applicationDirPath());
    const QStringList candidates = {
        appDir.absoluteFilePath(QStringLiteral("../..")),
        appDir.absoluteFilePath(QStringLiteral("../../..")),
        appDir.absoluteFilePath(QStringLiteral(".")),
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate + QStringLiteral("/map/calibration.json"))) {
            return QDir(candidate).absolutePath();
        }
    }
    return appDir.absolutePath();
}

} // namespace

QString assetRoot()
{
    static const QString root = resolveAssetRoot();
    return root;
}

QString mapImagePath()
{
    return assetRoot() + QStringLiteral("/map/gateway.webp");
}

QString calibrationPath()
{
    return assetRoot() + QStringLiteral("/map/calibration.json");
}

QString dataDir()
{
    return assetRoot() + QStringLiteral("/data");
}

QString settingsPath()
{
    const QString configDir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configDir);
    return configDir + QStringLiteral("/settings-v2.json");
}

} // namespace isle::paths
