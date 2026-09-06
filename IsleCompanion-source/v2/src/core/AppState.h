#pragma once

#include "core/Position.h"

#include <QObject>
#include <QVector>
#include <optional>

namespace isle {

class Settings;

class AppState : public QObject {
    Q_OBJECT
public:
    explicit AppState(Settings &settings, QObject *parent = nullptr);

    const Position *currentPosition() const;
    const Position *previousPosition() const;
    const Waypoint *activeWaypoint() const;
    const QVector<Position> &breadcrumbs() const { return m_breadcrumbs; }
    double lastMovementHeading() const { return m_lastHeading; }
    double lastMovementDistance() const { return m_lastDistance; }
    bool hasLastMovementHeading() const { return m_hasHeading; }

public slots:
    void updatePosition(const Position &position);
    void setWaypoint(const Waypoint &waypoint);
    void clearWaypoint();
    void clearBreadcrumbs();

signals:
    void positionChanged(const Position &current, const Position &previous);
    void breadcrumbsChanged();
    void waypointChanged();

private:
    Settings &m_settings;
    Position m_current;
    Position m_previous;
    Waypoint m_waypoint;
    bool m_hasCurrent = false;
    bool m_hasPrevious = false;
    bool m_hasWaypoint = false;
    bool m_hasHeading = false;
    double m_lastHeading = 0.0;
    double m_lastDistance = 0.0;
    QVector<Position> m_breadcrumbs;
};

} // namespace isle
