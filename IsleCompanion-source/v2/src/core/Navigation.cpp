#include "core/Navigation.h"

#include <QtMath>

namespace isle {
namespace {

const char *kCardinals[] = {
    "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
    "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW",
};

} // namespace

double planarDistance(double ax, double ay, double bx, double by)
{
    return std::hypot(bx - ax, by - ay);
}

std::optional<double> headingDegrees(double ax, double ay, double bx, double by)
{
    const double dx = bx - ax;
    const double dy = by - ay;
    if (qFuzzyIsNull(dx) && qFuzzyIsNull(dy)) {
        return std::nullopt;
    }
    double heading = qRadiansToDegrees(std::atan2(dx, dy));
    if (heading < 0.0) {
        heading += 360.0;
    }
    return heading;
}

QString cardinalDirection(std::optional<double> heading)
{
    if (!heading.has_value()) {
        return QStringLiteral("—");
    }
    const int index = int((*heading + 11.25) / 22.5) % 16;
    return QString::fromLatin1(kCardinals[index]);
}

QString formatDistance(double worldUnits)
{
    const double metres = worldUnits / 100.0;
    if (metres >= 1000.0) {
        return QStringLiteral("%1 km").arg(metres / 1000.0, 0, 'f', 2);
    }
    return QStringLiteral("%1 m").arg(metres, 0, 'f', 0);
}

NearestPoi nearestNamedPoi(const Position &player,
                           const LayerRepository &repo,
                           const QHash<QString, bool> &enabledLayers)
{
    NearestPoi best;
    bool found = false;
    const QStringList layers = {
        QStringLiteral("water"),
        QStringLiteral("locations"),
        QStringLiteral("food"),
        QStringLiteral("ai"),
        QStringLiteral("salt_licks"),
        QStringLiteral("spawns"),
    };
    for (const QString &layer : layers) {
        if (!enabledLayers.value(layer, true)) {
            continue;
        }
        for (const LayerItem &item : repo.layer(layer)) {
            if (!item.hasPosition || item.name.trimmed().isEmpty()) {
                continue;
            }
            if (item.raw.contains(QStringLiteral("show_label"))
                && !item.showLabel) {
                continue;
            }
            const double distance =
                planarDistance(player.x, player.y, item.position.x(), item.position.y());
            if (found && distance >= best.distance) {
                continue;
            }
            best.name = item.name;
            best.layer = layer;
            best.x = item.position.x();
            best.y = item.position.y();
            best.distance = distance;
            const auto heading =
                headingDegrees(player.x, player.y, item.position.x(), item.position.y());
            best.hasHeading = heading.has_value();
            best.heading = heading.value_or(0.0);
            found = true;
        }
    }
    if (!found) {
        best.name.clear();
    }
    return best;
}

} // namespace isle
