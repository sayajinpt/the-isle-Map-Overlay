#pragma once

#include "core/LayerRepository.h"
#include "core/Position.h"

#include <QHash>
#include <QString>
#include <optional>

namespace isle {

struct NearestPoi {
    QString name;
    QString layer;
    double x = 0.0;
    double y = 0.0;
    double distance = 0.0;
    double heading = 0.0;
    bool hasHeading = false;
};

double planarDistance(double ax, double ay, double bx, double by);
std::optional<double> headingDegrees(double ax, double ay, double bx, double by);
QString cardinalDirection(std::optional<double> heading);
QString formatDistance(double worldUnits);
NearestPoi nearestNamedPoi(const Position &player,
                           const LayerRepository &repo,
                           const QHash<QString, bool> &enabledLayers);

} // namespace isle
