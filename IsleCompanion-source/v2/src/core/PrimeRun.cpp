#include "core/PrimeRun.h"

#include <utility>

namespace isle {

bool pointInRing(double x, double y, const QVector<QPointF> &ring)
{
    if (ring.size() < 3) {
        return false;
    }
    bool inside = false;
    QPointF previous = ring.last();
    for (const QPointF &current : ring) {
        const bool intersects = ((current.y() > y) != (previous.y() > y))
            && (x
                < ((previous.x() - current.x()) * (y - current.y())
                       / (previous.y() - current.y() + 0.0)
                   + current.x()));
        if (intersects) {
            inside = !inside;
        }
        previous = current;
    }
    return inside;
}

bool zoneContainsPoint(const LayerItem &zone, double x, double y)
{
    for (const auto &ring : zone.polygons) {
        if (pointInRing(x, y, ring)) {
            return true;
        }
    }
    if (zone.hasPosition && zone.radius > 0.0) {
        const double dx = x - zone.position.x();
        const double dy = y - zone.position.y();
        return (dx * dx + dy * dy) <= (zone.radius * zone.radius);
    }
    return false;
}

bool countsForPrimeRun(const Position &position)
{
    return isLiveMapSource(position.source);
}

bool countsForPrimeRun(const Position &position, LiveMapProvider activeProvider)
{
    if (activeProvider == LiveMapProvider::VoiceIsland) {
        return position.source == PositionSource::VoiceIsland;
    }
    return position.source == PositionSource::Bosch;
}

QStringList PrimeRunProgress::summaryLines() const
{
    return {
        QStringLiteral("Sanctuary: %1/%2 (%3)")
            .arg(sanctuaryVisited)
            .arg(sanctuaryTotal)
            .arg(sanctuaryVisited >= kPrimeSanctuaryGoal
                     ? QStringLiteral("done")
                     : QStringLiteral("need %1").arg(kPrimeSanctuaryGoal)),
        QStringLiteral("Migrations: %1/%2 (%3)")
            .arg(migrationVisited)
            .arg(migrationTotal)
            .arg(migrationVisited >= kPrimeMigrationGoal
                     ? QStringLiteral("done")
                     : QStringLiteral("need %1").arg(kPrimeMigrationGoal)),
        QStringLiteral("Patrol zones: %1/%2 (%3)")
            .arg(patrolVisited)
            .arg(patrolTotal)
            .arg(patrolVisited >= kPrimePatrolGoal
                     ? QStringLiteral("done")
                     : QStringLiteral("need %1").arg(kPrimePatrolGoal)),
    };
}

QVector<ZoneVisit> discoverZoneVisits(const LayerRepository &repo, double x, double y)
{
    QVector<ZoneVisit> visits;
    const QStringList layers = {
        QStringLiteral("sanctuaries"),
        QStringLiteral("migrations"),
        QStringLiteral("patrol_zones"),
    };
    for (const QString &layer : layers) {
        for (const LayerItem &zone : repo.layer(layer)) {
            if (!zoneContainsPoint(zone, x, y)) {
                continue;
            }
            visits.push_back({zoneStableId(layer, zone), layer, zone.name});
        }
    }
    return visits;
}

QVector<ZoneVisit> recordVisits(Settings &settings, const QVector<ZoneVisit> &visits)
{
    if (!settings.data().primeRunActive) {
        return {};
    }
    QVector<ZoneVisit> fresh;
    auto &known = settings.data().visitedZoneIds;
    for (const ZoneVisit &visit : visits) {
        if (known.contains(visit.zoneId)) {
            continue;
        }
        known.insert(visit.zoneId);
        fresh.push_back(visit);
    }
    if (!fresh.isEmpty()) {
        settings.notifyLayersUpdated();
    }
    return fresh;
}

PrimeRunProgress primeRunProgress(const LayerRepository &repo, const Settings &settings)
{
    const auto &visited = settings.data().visitedZoneIds;
    auto count = [&](const QString &layer) -> std::pair<int, int> {
        const auto &items = repo.layer(layer);
        int hit = 0;
        for (const LayerItem &zone : items) {
            if (visited.contains(zoneStableId(layer, zone))) {
                ++hit;
            }
        }
        return {hit, items.size()};
    };
    const auto sanctuary = count(QStringLiteral("sanctuaries"));
    const auto migration = count(QStringLiteral("migrations"));
    const auto patrol = count(QStringLiteral("patrol_zones"));
    return {
        sanctuary.first,
        sanctuary.second,
        migration.first,
        migration.second,
        patrol.first,
        patrol.second,
    };
}

void clearPrimeRun(Settings &settings)
{
    settings.data().visitedZoneIds.clear();
    settings.data().primeRunActive = true;
    settings.notifyLayersUpdated();
}

} // namespace isle
