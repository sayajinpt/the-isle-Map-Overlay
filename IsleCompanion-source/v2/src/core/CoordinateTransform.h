#pragma once

#include <QPointF>
#include <QString>
#include <utility>

namespace isle {

struct MapCalibration {
    double worldMinX = -607000.0;
    double worldMaxX = 509000.0;
    double worldMinY = -505000.0;
    double worldMaxY = 607000.0;
    double pixelMinX = 0.0;
    double pixelMaxX = 7800.0;
    double pixelMinY = 0.0;
    double pixelMaxY = 7817.0;
    bool invertY = false;
    bool invertX = false;
    bool swapAxes = true;

    QPointF worldToPixel(double x, double y) const;
    std::pair<double, double> pixelToWorld(double pixelX, double pixelY) const;
};

MapCalibration loadCalibration(const QString &path);
bool saveCalibration(const QString &path, const MapCalibration &calibration);

} // namespace isle
