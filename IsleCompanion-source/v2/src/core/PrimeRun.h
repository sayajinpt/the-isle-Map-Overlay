#pragma once

#include "core/LayerRepository.h"
#include "core/Position.h"
#include "core/Settings.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace isle {

inline constexpr int kPrimeSanctuaryGoal = 1;
inline constexpr int kPrimeMigrationGoal = 2;
inline constexpr int kPrimePatrolGoal = 4;

inline QString zoneStableId(const QString &layer, const LayerItem &item)
{
    if (!item.sourceId.trimmed().isEmpty()) {
        return layer + QLatin1Char(':') + item.sourceId.trimmed();
    }
    return layer + QLatin1Char(':') + item.name.trimmed();
}

bool pointInRing(double x, double y, const QVector<QPointF> &ring);
bool zoneContainsPoint(const LayerItem &zone, double x, double y);
bool countsForPrimeRun(const Position &position);
bool countsForPrimeRun(const Position &position, LiveMapProvider activeProvider);

struct ZoneVisit {
    QString zoneId;
    QString layer;
    QString name;
};

struct PrimeRunProgress {
    int sanctuaryVisited = 0;
    int sanctuaryTotal = 0;
    int migrationVisited = 0;
    int migrationTotal = 0;
    int patrolVisited = 0;
    int patrolTotal = 0;

    QStringList summaryLines() const;
};

QVector<ZoneVisit> discoverZoneVisits(const LayerRepository &repo, double x, double y);
QVector<ZoneVisit> recordVisits(Settings &settings, const QVector<ZoneVisit> &visits);
PrimeRunProgress primeRunProgress(const LayerRepository &repo, const Settings &settings);
void clearPrimeRun(Settings &settings);

} // namespace isle
