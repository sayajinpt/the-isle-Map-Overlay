#include "core/CoordinateTransform.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace isle {

QPointF MapCalibration::worldToPixel(double x, double y) const
{
    const double xRatio = (x - worldMinX) / (worldMaxX - worldMinX);
    const double yRatio = (y - worldMinY) / (worldMaxY - worldMinY);
    double pixelXRatio = swapAxes ? yRatio : xRatio;
    double pixelYRatio = swapAxes ? xRatio : yRatio;
    if (invertX) {
        pixelXRatio = 1.0 - pixelXRatio;
    }
    if (invertY) {
        pixelYRatio = 1.0 - pixelYRatio;
    }
    const double pixelX = pixelMinX + pixelXRatio * (pixelMaxX - pixelMinX);
    const double pixelY = pixelMinY + pixelYRatio * (pixelMaxY - pixelMinY);
    return {pixelX, pixelY};
}

std::pair<double, double> MapCalibration::pixelToWorld(double pixelX, double pixelY) const
{
    double pixelXRatio = (pixelX - pixelMinX) / (pixelMaxX - pixelMinX);
    double pixelYRatio = (pixelY - pixelMinY) / (pixelMaxY - pixelMinY);
    if (invertX) {
        pixelXRatio = 1.0 - pixelXRatio;
    }
    if (invertY) {
        pixelYRatio = 1.0 - pixelYRatio;
    }
    const double xRatio = swapAxes ? pixelYRatio : pixelXRatio;
    const double yRatio = swapAxes ? pixelXRatio : pixelYRatio;
    const double x = worldMinX + xRatio * (worldMaxX - worldMinX);
    const double y = worldMinY + yRatio * (worldMaxY - worldMinY);
    return {x, y};
}

MapCalibration loadCalibration(const QString &path)
{
    MapCalibration calibration;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return calibration;
    }
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return calibration;
    }
    const QJsonObject root = document.object();
    const QJsonObject world = root.value(QStringLiteral("world_bounds")).toObject();
    const QJsonObject pixels = root.value(QStringLiteral("pixel_bounds")).toObject();
    calibration.worldMinX = world.value(QStringLiteral("min_x")).toDouble(calibration.worldMinX);
    calibration.worldMaxX = world.value(QStringLiteral("max_x")).toDouble(calibration.worldMaxX);
    calibration.worldMinY = world.value(QStringLiteral("min_y")).toDouble(calibration.worldMinY);
    calibration.worldMaxY = world.value(QStringLiteral("max_y")).toDouble(calibration.worldMaxY);
    calibration.pixelMinX = pixels.value(QStringLiteral("min_x")).toDouble(calibration.pixelMinX);
    calibration.pixelMaxX = pixels.value(QStringLiteral("max_x")).toDouble(calibration.pixelMaxX);
    calibration.pixelMinY = pixels.value(QStringLiteral("min_y")).toDouble(calibration.pixelMinY);
    calibration.pixelMaxY = pixels.value(QStringLiteral("max_y")).toDouble(calibration.pixelMaxY);
    calibration.invertY = root.value(QStringLiteral("invert_y")).toBool(calibration.invertY);
    calibration.invertX = root.value(QStringLiteral("invert_x")).toBool(calibration.invertX);
    calibration.swapAxes = root.value(QStringLiteral("swap_axes")).toBool(calibration.swapAxes);
    return calibration;
}

bool saveCalibration(const QString &path, const MapCalibration &calibration)
{
    QJsonObject world;
    world.insert(QStringLiteral("min_x"), calibration.worldMinX);
    world.insert(QStringLiteral("max_x"), calibration.worldMaxX);
    world.insert(QStringLiteral("min_y"), calibration.worldMinY);
    world.insert(QStringLiteral("max_y"), calibration.worldMaxY);
    QJsonObject pixels;
    pixels.insert(QStringLiteral("min_x"), calibration.pixelMinX);
    pixels.insert(QStringLiteral("max_x"), calibration.pixelMaxX);
    pixels.insert(QStringLiteral("min_y"), calibration.pixelMinY);
    pixels.insert(QStringLiteral("max_y"), calibration.pixelMaxY);
    QJsonObject root;
    root.insert(
        QStringLiteral("description"),
        QStringLiteral(
            "Adjust these values to align Gateway world coordinates with the local map image."));
    root.insert(QStringLiteral("world_bounds"), world);
    root.insert(QStringLiteral("pixel_bounds"), pixels);
    root.insert(QStringLiteral("invert_y"), calibration.invertY);
    root.insert(QStringLiteral("invert_x"), calibration.invertX);
    root.insert(QStringLiteral("swap_axes"), calibration.swapAxes);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

} // namespace isle
