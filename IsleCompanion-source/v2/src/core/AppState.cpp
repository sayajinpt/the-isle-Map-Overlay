#include "core/AppState.h"

#include "core/Navigation.h"
#include "core/Settings.h"

namespace isle {

AppState::AppState(Settings &settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
}

const Position *AppState::currentPosition() const
{
    return m_hasCurrent ? &m_current : nullptr;
}

const Position *AppState::previousPosition() const
{
    return m_hasPrevious ? &m_previous : nullptr;
}

const Waypoint *AppState::activeWaypoint() const
{
    return m_hasWaypoint ? &m_waypoint : nullptr;
}

void AppState::updatePosition(const Position &position)
{
    if (m_hasCurrent
        && qFuzzyCompare(m_current.x, position.x)
        && qFuzzyCompare(m_current.y, position.y)
        && qFuzzyCompare(m_current.z, position.z)) {
        return;
    }

    if (m_hasCurrent) {
        m_previous = m_current;
        m_hasPrevious = true;
        m_lastDistance = planarDistance(m_previous.x, m_previous.y, position.x, position.y);
        const auto heading = headingDegrees(m_previous.x, m_previous.y, position.x, position.y);
        m_hasHeading = heading.has_value();
        m_lastHeading = heading.value_or(0.0);
    }
    m_current = position;
    m_hasCurrent = true;

    if (m_settings.data().breadcrumbsEnabled) {
        m_breadcrumbs.push_back(position);
        const int maxPoints = m_settings.data().breadcrumbMaxPoints;
        while (m_breadcrumbs.size() > maxPoints) {
            m_breadcrumbs.removeFirst();
        }
        emit breadcrumbsChanged();
    }

    emit positionChanged(m_current, m_hasPrevious ? m_previous : Position{});
}

void AppState::setWaypoint(const Waypoint &waypoint)
{
    m_waypoint = waypoint;
    m_hasWaypoint = true;
    emit waypointChanged();
}

void AppState::clearWaypoint()
{
    if (!m_hasWaypoint) {
        return;
    }
    m_hasWaypoint = false;
    emit waypointChanged();
}

void AppState::clearBreadcrumbs()
{
    if (m_breadcrumbs.isEmpty()) {
        return;
    }
    m_breadcrumbs.clear();
    emit breadcrumbsChanged();
}

} // namespace isle
