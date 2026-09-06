#include "ui/MapView.h"

#include "core/Paths.h"
#include "core/PrimeRun.h"

#include <QAbstractGraphicsShapeItem>
#include <QBrush>
#include <QFrame>
#include <QGraphicsEllipseItem>
#include <QGraphicsItemGroup>
#include <QGraphicsPathItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QImage>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QResizeEvent>
#include <QWheelEvent>

namespace isle {
namespace {

// Salt / ocean / general shoreline raster tint.
const QColor kSaltWaterTint(28, 96, 138);
// Fresh drinkable sources (lakes, ponds, puddles).
const QColor kFreshWaterMarker(48, 214, 255);
// Flowing fresh water (rivers, falls, cascades, deltas).
const QColor kRiverWaterMarker(34, 240, 190);
// Coastal / brackish named spots.
const QColor kCoastalWaterMarker(110, 160, 205);

const QColor kMigrationFill(0, 204, 119, 78);
const QColor kMigrationOutline(0, 200, 110);
const QColor kPatrolFill(255, 0, 0, 60);
const QColor kPatrolOutline(235, 45, 45);
const QColor kSanctuaryFill(255, 153, 255, 82);
const QColor kSanctuaryOutline(230, 120, 230);
const QColor kVisitedOutline(QStringLiteral("#FFD166"));

QColor opaqueFrameColor(const QColor &color)
{
    QColor frame = color;
    frame.setAlpha(255);
    return frame;
}

QColor translucentFillColor(const QColor &color, int alpha)
{
    QColor fill = color;
    fill.setAlpha(alpha);
    return fill;
}

QColor waterSourceColor(const QString &name)
{
    const QString n = name.toLower();
    if (n.contains(QLatin1String("coastal")) || n.contains(QLatin1String("salt"))
        || n.contains(QLatin1String("ocean")) || n.contains(QLatin1String("sea"))) {
        return kCoastalWaterMarker;
    }
    if (n.contains(QLatin1String("river")) || n.contains(QLatin1String("falls"))
        || n.contains(QLatin1String("cascade")) || n.contains(QLatin1String("delta"))) {
        return kRiverWaterMarker;
    }
    return kFreshWaterMarker;
}

QString waterSourceKindLabel(const QString &name)
{
    const QString n = name.toLower();
    if (n.contains(QLatin1String("coastal")) || n.contains(QLatin1String("salt"))
        || n.contains(QLatin1String("ocean")) || n.contains(QLatin1String("sea"))) {
        return QStringLiteral("Coastal / salt-adjacent");
    }
    if (n.contains(QLatin1String("river")) || n.contains(QLatin1String("falls"))
        || n.contains(QLatin1String("cascade")) || n.contains(QLatin1String("delta"))) {
        return QStringLiteral("River / flowing fresh water");
    }
    return QStringLiteral("Fresh water source");
}

QPixmap tintSaltWaterOverlay(const QPixmap &source)
{
    QImage image = source.toImage().convertToFormat(QImage::Format_ARGB32);
    const int tr = kSaltWaterTint.red();
    const int tg = kSaltWaterTint.green();
    const int tb = kSaltWaterTint.blue();
    for (int y = 0; y < image.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgb px = line[x];
            const int a = qAlpha(px);
            if (a < 8) {
                continue;
            }
            const int lum = (qRed(px) * 30 + qGreen(px) * 59 + qBlue(px) * 11) / 100;
            // Keep source brightness, push hue toward salt/ocean blue.
            const int r = qBound(0, (lum * 55 + tr * 200) / 255, 255);
            const int g = qBound(0, (lum * 70 + tg * 185) / 255, 255);
            const int b = qBound(0, (lum * 85 + tb * 170) / 255, 255);
            line[x] = qRgba(r, g, b, a);
        }
    }
    return QPixmap::fromImage(image);
}

QPolygonF updraftArrow()
{
    return QPolygonF({
        QPointF(0, -17),
        QPointF(-7, -5),
        QPointF(-1, -7),
        QPointF(-3, 1),
        QPointF(0, -1),
        QPointF(3, 1),
        QPointF(1, -7),
        QPointF(7, -5),
    });
}

} // namespace

MapView::MapView(Settings &settings,
                 AppState &state,
                 LayerRepository &repository,
                 MapCalibration calibration,
                 bool compact,
                 QWidget *parent)
    : QGraphicsView(parent)
    , m_settings(settings)
    , m_state(state)
    , m_repository(repository)
    , m_calibration(std::move(calibration))
    , m_compact(compact)
{
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform
                   | QPainter::TextAntialiasing);
    setDragMode(ScrollHandDrag);
    setTransformationAnchor(AnchorUnderMouse);
    setResizeAnchor(AnchorViewCenter);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setBackgroundBrush(QColor(QStringLiteral("#081015")));
    setFrameShape(QFrame::NoFrame);
    createGroups();

    connect(&m_state, &AppState::positionChanged, this, [this](const Position &, const Position &) {
        onPositionChanged();
    });
    connect(&m_state, &AppState::breadcrumbsChanged, this, &MapView::rebuildBreadcrumbs);
    connect(&m_state, &AppState::waypointChanged, this, &MapView::onWaypointChanged);
    connect(&m_settings, &Settings::changed, this, &MapView::onSettingsChanged);
    connect(&m_settings, &Settings::layersChanged, this, &MapView::onLayersChanged);
}

void MapView::createGroups()
{
    const QStringList names = {
        QStringLiteral("migrations"),    QStringLiteral("patrol_zones"),
        QStringLiteral("sanctuaries"),   QStringLiteral("water"),
        QStringLiteral("updrafts"),      QStringLiteral("locations"),
        QStringLiteral("food"),          QStringLiteral("ai"),
        QStringLiteral("salt_licks"),    QStringLiteral("spawns"),
        QStringLiteral("breadcrumbs"),   QStringLiteral("waypoint_route"),
        QStringLiteral("waypoint"),      QStringLiteral("player"),
    };
    int z = 5;
    for (const QString &name : names) {
        auto *group = new QGraphicsItemGroup();
        group->setZValue(z++);
        m_scene->addItem(group);
        m_groups.insert(name, group);
    }
    m_groups[QStringLiteral("player")]->setZValue(50);
    m_groups[QStringLiteral("waypoint")]->setZValue(45);
    m_groups[QStringLiteral("waypoint_route")]->setZValue(40);
}

void MapView::clearGroup(const QString &name)
{
    QGraphicsItemGroup *group = m_groups.value(name, nullptr);
    if (group == nullptr) {
        return;
    }
    const auto children = group->childItems();
    for (QGraphicsItem *child : children) {
        group->removeFromGroup(child);
        delete child;
    }
}

void MapView::reloadMapImage(const QString &path)
{
    m_mapPixmap = QPixmap(path);
    if (m_mapPixmap.isNull()) {
        m_mapPixmap = QPixmap(1600, 1600);
        m_mapPixmap.fill(QColor(QStringLiteral("#10242a")));
    }
    if (m_mapItem == nullptr) {
        m_mapItem = m_scene->addPixmap(m_mapPixmap);
        m_mapItem->setZValue(0);
    } else {
        m_mapItem->setPixmap(m_mapPixmap);
    }
    m_scene->setSceneRect(m_mapPixmap.rect());
    renderStaticLayers();
    rebuildBreadcrumbs();
    rebuildWaypoint();
    rebuildPlayerMarker();
    fitWholeMap();
    if (m_compact && m_settings.data().playerCentered && m_state.currentPosition() != nullptr) {
        recenterOnPlayer();
    }
}

void MapView::setCalibration(const MapCalibration &calibration)
{
    m_calibration = calibration;
    renderStaticLayers();
    rebuildBreadcrumbs();
    rebuildWaypoint();
    rebuildPlayerMarker();
    if (m_compact && m_settings.data().playerCentered && m_state.currentPosition() != nullptr) {
        recenterOnPlayer();
    }
}

void MapView::fitWholeMap()
{
    fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
    updateDetailVisibility();
}

void MapView::renderStaticLayers()
{
    renderZoneLayer(QStringLiteral("migrations"), kMigrationFill, kMigrationOutline);
    renderZoneLayer(QStringLiteral("patrol_zones"), kPatrolFill, kPatrolOutline);
    renderZoneLayer(QStringLiteral("sanctuaries"), kSanctuaryFill, kSanctuaryOutline);
    clearGroup(QStringLiteral("water"));
    renderWaterOverlay();
    renderWaterSources();
    renderUpdrafts();
    const bool labels = !m_compact || m_settings.data().miniMapShowPoiLabels;
    renderPointLayer(QStringLiteral("locations"), QColor(QStringLiteral("#e5e7eb")), labels);
    renderPointLayer(QStringLiteral("food"), QColor(QStringLiteral("#8dd66b")), labels);
    renderPointLayer(QStringLiteral("ai"), QColor(QStringLiteral("#d991f0")), labels);
    renderPointLayer(QStringLiteral("salt_licks"), QColor(QStringLiteral("#e7c574")), labels);
    renderPointLayer(QStringLiteral("spawns"), QColor(QStringLiteral("#ff9f68")), labels);
    refreshVisibility();
    updateDetailVisibility();
}

QPolygonF MapView::polygonFromWorld(const QVector<QPointF> &ring) const
{
    QPolygonF polygon;
    polygon.reserve(ring.size());
    for (const QPointF &world : ring) {
        polygon << m_calibration.worldToPixel(world.x(), world.y());
    }
    return polygon;
}

double MapView::worldRadiusToPixels(double radius) const
{
    const QPointF p0 = m_calibration.worldToPixel(0.0, 0.0);
    const QPointF px = m_calibration.worldToPixel(radius, 0.0);
    const QPointF py = m_calibration.worldToPixel(0.0, radius);
    return (QLineF(p0, px).length() + QLineF(p0, py).length()) * 0.5;
}

void MapView::renderZoneLayer(const QString &layer,
                              const QColor &fillDefault,
                              const QColor &outlineDefault)
{
    clearGroup(layer);
    QGraphicsItemGroup *group = m_groups.value(layer, nullptr);
    if (group == nullptr) {
        return;
    }
    const auto &visited = m_settings.data().visitedZoneIds;
    for (const LayerItem &zone : m_repository.layer(layer)) {
        if (layer == QStringLiteral("migrations")
            && !m_settings.data().individualMigrations.value(zone.name, true)) {
            continue;
        }
        if (layer == QStringLiteral("patrol_zones")
            && !m_settings.data().individualPatrols.value(zone.name, true)) {
            continue;
        }
        if (layer == QStringLiteral("sanctuaries")
            && !m_settings.data().individualSanctuaries.value(zone.name, true)) {
            continue;
        }
        const QString zoneId = zoneStableId(layer, zone);
        const bool visitedHere = visited.contains(zoneId);
        const QColor baseColor = zone.color.isEmpty() ? fillDefault : QColor(zone.color);
        QColor fill = translucentFillColor(baseColor, fillDefault.alpha());
        if (visitedHere) {
            fill = translucentFillColor(baseColor.lighter(118), qMin(255, fillDefault.alpha() + 28));
        }
        // Opaque frame so zone edges stay readable over translucent fills.
        const QColor outlineColor =
            visitedHere ? opaqueFrameColor(kVisitedOutline)
                        : opaqueFrameColor(zone.color.isEmpty() ? outlineDefault : QColor(zone.color));
        QString tip = zone.name;
        if (visitedHere) {
            tip += QStringLiteral("\nVisited (prime run)");
        }
        if (!zone.description.isEmpty()) {
            tip += QLatin1Char('\n') + zone.description;
        }

        auto applyZoneStyle = [&](QAbstractGraphicsShapeItem *item) {
            item->setBrush(QBrush(fill));
            QPen outline(outlineColor, visitedHere ? 3.2 : 2.4);
            outline.setCosmetic(true);
            outline.setJoinStyle(Qt::RoundJoin);
            outline.setCapStyle(Qt::RoundCap);
            item->setPen(outline);
            item->setToolTip(tip);
            group->addToGroup(item);
        };

        for (const auto &ring : zone.polygons) {
            const QPolygonF polygon = polygonFromWorld(ring);
            if (polygon.size() < 3) {
                continue;
            }
            applyZoneStyle(new QGraphicsPolygonItem(polygon));
        }
        if (zone.polygons.isEmpty() && zone.hasPosition && zone.radius > 0.0) {
            const QPointF center =
                m_calibration.worldToPixel(zone.position.x(), zone.position.y());
            const double r = worldRadiusToPixels(zone.radius);
            applyZoneStyle(new QGraphicsEllipseItem(center.x() - r, center.y() - r, r * 2.0, r * 2.0));
        }
    }
}

void MapView::renderWaterOverlay()
{
    QGraphicsItemGroup *group = m_groups.value(QStringLiteral("water"), nullptr);
    if (group == nullptr) {
        return;
    }
    const QString waterPath = paths::mapImagePath().replace(
        QStringLiteral("gateway.webp"), QStringLiteral("gateway_water.webp"));
    static QString cachedPath;
    static QPixmap cachedTinted;
    if (cachedPath != waterPath || cachedTinted.isNull()) {
        QPixmap water(waterPath);
        if (water.isNull() || water.size() != m_mapPixmap.size()) {
            return;
        }
        cachedTinted = tintSaltWaterOverlay(water);
        cachedPath = waterPath;
    }
    if (cachedTinted.isNull() || cachedTinted.size() != m_mapPixmap.size()) {
        return;
    }
    auto *item = new QGraphicsPixmapItem(cachedTinted);
    item->setToolTip(QStringLiteral("Salt / ocean / shoreline water"));
    item->setOpacity(0.90);
    group->addToGroup(item);
}

void MapView::renderWaterSources()
{
    QGraphicsItemGroup *group = m_groups.value(QStringLiteral("water"), nullptr);
    if (group == nullptr) {
        return;
    }
    const bool drawLabels = !m_compact || m_settings.data().miniMapShowPoiLabels;
    for (const LayerItem &item : m_repository.layer(QStringLiteral("water"))) {
        if (!item.hasPosition) {
            continue;
        }
        const QPointF pixel = m_calibration.worldToPixel(item.position.x(), item.position.y());
        const QColor fill = waterSourceColor(item.name);
        auto *halo = new QGraphicsEllipseItem(-7, -7, 14, 14);
        halo->setPos(pixel);
        halo->setBrush(QBrush(QColor(fill.red(), fill.green(), fill.blue(), 70)));
        halo->setPen(Qt::NoPen);
        halo->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
        auto *dot = new QGraphicsEllipseItem(-4.5, -4.5, 9, 9);
        dot->setPos(pixel);
        dot->setBrush(QBrush(fill));
        QPen pen(QColor(245, 252, 255), 1.4);
        pen.setCosmetic(true);
        dot->setPen(pen);
        dot->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
        const QString tip =
            QStringLiteral("%1\n%2").arg(item.name, waterSourceKindLabel(item.name));
        halo->setToolTip(tip);
        dot->setToolTip(tip);
        group->addToGroup(halo);
        group->addToGroup(dot);
        if (drawLabels && !item.name.isEmpty()
            && (item.showLabel || !item.raw.contains(QStringLiteral("show_label")))) {
            auto *label = new QGraphicsSimpleTextItem(item.name);
            label->setBrush(QBrush(fill.lighter(130)));
            label->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
            label->setPos(pixel + QPointF(8, -8));
            label->setData(0, QStringLiteral("poi_label"));
            group->addToGroup(label);
        }
    }
}

void MapView::renderPointLayer(const QString &layer, const QColor &color, bool drawLabels)
{
    clearGroup(layer);
    QGraphicsItemGroup *group = m_groups.value(layer, nullptr);
    if (group == nullptr) {
        return;
    }
    for (const LayerItem &item : m_repository.layer(layer)) {
        if (!item.hasPosition) {
            continue;
        }
        const QPointF pixel = m_calibration.worldToPixel(item.position.x(), item.position.y());
        auto *dot = new QGraphicsEllipseItem(-4, -4, 8, 8);
        dot->setPos(pixel);
        dot->setBrush(QBrush(color));
        QPen pen(Qt::black, 1);
        pen.setCosmetic(true);
        dot->setPen(pen);
        dot->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
        dot->setToolTip(item.name);
        group->addToGroup(dot);
        if (drawLabels && !item.name.isEmpty()
            && (item.showLabel || !item.raw.contains(QStringLiteral("show_label")))) {
            auto *label = new QGraphicsSimpleTextItem(item.name);
            label->setBrush(QBrush(Qt::white));
            label->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
            label->setPos(pixel + QPointF(6, -6));
            label->setData(0, QStringLiteral("poi_label"));
            group->addToGroup(label);
        }
    }
}

void MapView::renderUpdrafts()
{
    clearGroup(QStringLiteral("updrafts"));
    QGraphicsItemGroup *group = m_groups.value(QStringLiteral("updrafts"), nullptr);
    if (group == nullptr) {
        return;
    }
    for (const LayerItem &point : m_repository.layer(QStringLiteral("updrafts"))) {
        if (!point.hasPosition) {
            continue;
        }
        const QPointF pixel = m_calibration.worldToPixel(point.position.x(), point.position.y());
        auto *shadow = new QGraphicsPolygonItem(updraftArrow());
        shadow->setBrush(QBrush(QColor(0, 0, 0, 190)));
        shadow->setPen(QPen(Qt::NoPen));
        shadow->setPos(pixel + QPointF(2, 2));
        shadow->setOpacity(0.75);
        shadow->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
        auto *marker = new QGraphicsPolygonItem(updraftArrow());
        marker->setBrush(QBrush(QColor(QStringLiteral("#FF6600"))));
        QPen outline(Qt::white, 1.5);
        outline.setCosmetic(true);
        marker->setPen(outline);
        marker->setPos(pixel);
        marker->setOpacity(0.85);
        marker->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
        marker->setToolTip(point.name);
        group->addToGroup(shadow);
        group->addToGroup(marker);
    }
}

void MapView::refreshVisibility()
{
    for (auto it = m_groups.begin(); it != m_groups.end(); ++it) {
        if (it.key() == QStringLiteral("waypoint")
            || it.key() == QStringLiteral("waypoint_route")) {
            it.value()->setVisible(true);
            continue;
        }
        it.value()->setVisible(m_settings.data().layers.value(it.key(), true));
    }
}

void MapView::updateDetailVisibility()
{
    const bool showLabels = transform().m11() > 0.12
        && (!m_compact || m_settings.data().miniMapShowPoiLabels);
    for (const QString &layer :
         {QStringLiteral("locations"),
          QStringLiteral("food"),
          QStringLiteral("ai"),
          QStringLiteral("salt_licks"),
          QStringLiteral("spawns"),
          QStringLiteral("water")}) {
        QGraphicsItemGroup *group = m_groups.value(layer, nullptr);
        if (group == nullptr) {
            continue;
        }
        for (QGraphicsItem *child : group->childItems()) {
            if (child->data(0).toString() == QStringLiteral("poi_label")) {
                child->setVisible(showLabels);
            }
        }
    }
}

void MapView::onPositionChanged()
{
    rebuildPlayerMarker();
    rebuildWaypoint();
    if (m_compact && m_settings.data().playerCentered) {
        recenterOnPlayer();
    }
}

void MapView::onSettingsChanged()
{
    refreshVisibility();
    if (m_compact && m_settings.data().playerCentered) {
        recenterOnPlayer();
    }
}

void MapView::onLayersChanged()
{
    renderStaticLayers();
    rebuildBreadcrumbs();
    rebuildWaypoint();
    rebuildPlayerMarker();
}

void MapView::onWaypointChanged()
{
    rebuildWaypoint();
}

void MapView::ensureFollowZoom()
{
    const QRect viewportRect = viewport()->rect();
    const QRectF scene = m_scene->sceneRect();
    if (viewportRect.width() < 10 || scene.width() < 1.0) {
        return;
    }
    const qreal fitScale = qMin(viewportRect.width() / scene.width(),
                                viewportRect.height() / scene.height());
    const qreal current = qAbs(transform().m11());
    if (current > fitScale * 1.2) {
        return;
    }
    const qreal targetSpan = qMax(900.0, scene.width() / 8.0);
    const qreal targetScale = viewportRect.width() / targetSpan;
    if (targetScale <= current * 1.01) {
        return;
    }
    scale(targetScale / current, targetScale / current);
    updateDetailVisibility();
}

void MapView::recenterOnPlayer()
{
    const Position *position = m_state.currentPosition();
    if (position == nullptr) {
        return;
    }
    if (m_compact && m_settings.data().playerCentered) {
        ensureFollowZoom();
    }
    const QPointF point = m_calibration.worldToPixel(position->x, position->y);
    const auto previous = transformationAnchor();
    setTransformationAnchor(AnchorViewCenter);
    centerOn(point);
    setTransformationAnchor(previous);
}

void MapView::rebuildPlayerMarker()
{
    clearGroup(QStringLiteral("player"));
    QGraphicsItemGroup *group = m_groups.value(QStringLiteral("player"));
    if (group == nullptr) {
        return;
    }
    if (const Position *previous = m_state.previousPosition()) {
        const QPointF point = m_calibration.worldToPixel(previous->x, previous->y);
        auto *ghost = new QGraphicsEllipseItem(-6, -6, 12, 12);
        ghost->setPos(point);
        ghost->setBrush(QBrush(QColor(116, 136, 148, 205)));
        QPen pen(QColor(QStringLiteral("#F0F5F7")), 2);
        pen.setCosmetic(true);
        ghost->setPen(pen);
        ghost->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
        ghost->setToolTip(QStringLiteral("Previous position"));
        group->addToGroup(ghost);
    }
    const Position *position = m_state.currentPosition();
    if (position == nullptr) {
        return;
    }
    const QPointF point = m_calibration.worldToPixel(position->x, position->y);
    auto *halo = new QGraphicsEllipseItem(-13, -13, 26, 26);
    halo->setPos(point);
    halo->setBrush(QBrush(QColor(38, 214, 239, 95)));
    halo->setPen(QPen(Qt::NoPen));
    halo->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    auto *player = new QGraphicsEllipseItem(-6, -6, 12, 12);
    player->setPos(point);
    player->setBrush(QBrush(QColor(QStringLiteral("#19D3EE"))));
    QPen pen(Qt::white, 2);
    pen.setCosmetic(true);
    player->setPen(pen);
    player->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    player->setToolTip(QStringLiteral("Player position"));
    group->addToGroup(halo);
    group->addToGroup(player);
}

void MapView::rebuildBreadcrumbs()
{
    clearGroup(QStringLiteral("breadcrumbs"));
    QGraphicsItemGroup *group = m_groups.value(QStringLiteral("breadcrumbs"));
    if (group == nullptr || !m_settings.data().breadcrumbsEnabled) {
        return;
    }
    const auto &crumbs = m_state.breadcrumbs();
    if (m_settings.data().breadcrumbConnectLines && crumbs.size() >= 2) {
        const Position &a = crumbs[crumbs.size() - 2];
        const Position &b = crumbs[crumbs.size() - 1];
        QPainterPath path(m_calibration.worldToPixel(a.x, a.y));
        path.lineTo(m_calibration.worldToPixel(b.x, b.y));
        auto *line = new QGraphicsPathItem(path);
        QPen pen(QColor(255, 230, 161, 180), 2);
        pen.setCosmetic(true);
        line->setPen(pen);
        group->addToGroup(line);
    }
    for (const Position &crumb : crumbs) {
        const QPointF point = m_calibration.worldToPixel(crumb.x, crumb.y);
        auto *dot = new QGraphicsEllipseItem(-3, -3, 6, 6);
        dot->setPos(point);
        dot->setBrush(QBrush(QColor(QStringLiteral("#FFE6A1"))));
        QPen pen(QColor(QStringLiteral("#071115")), 1.5);
        pen.setCosmetic(true);
        dot->setPen(pen);
        dot->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
        group->addToGroup(dot);
    }
}

void MapView::rebuildWaypoint()
{
    clearGroup(QStringLiteral("waypoint"));
    clearGroup(QStringLiteral("waypoint_route"));
    const Waypoint *waypoint = m_state.activeWaypoint();
    if (waypoint == nullptr) {
        return;
    }
    const QPointF point = m_calibration.worldToPixel(waypoint->x, waypoint->y);
    if (const Position *player = m_state.currentPosition()) {
        QPainterPath path(m_calibration.worldToPixel(player->x, player->y));
        path.lineTo(point);
        auto *under = new QGraphicsPathItem(path);
        QPen underPen(QColor(5, 12, 16, 220), 7);
        underPen.setCosmetic(true);
        underPen.setCapStyle(Qt::RoundCap);
        under->setPen(underPen);
        auto *route = new QGraphicsPathItem(path);
        QPen routePen(QColor(QStringLiteral("#ff5d6c")), 3);
        routePen.setCosmetic(true);
        routePen.setCapStyle(Qt::RoundCap);
        route->setPen(routePen);
        m_groups[QStringLiteral("waypoint_route")]->addToGroup(under);
        m_groups[QStringLiteral("waypoint_route")]->addToGroup(route);
    }
    auto *marker = new QGraphicsEllipseItem(-8, -8, 16, 16);
    marker->setPos(point);
    marker->setBrush(QBrush(QColor(QStringLiteral("#ff5d6c"))));
    QPen pen(Qt::white, 2);
    pen.setCosmetic(true);
    marker->setPen(pen);
    marker->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    marker->setToolTip(waypoint->name);
    marker->setData(0, QStringLiteral("active_waypoint"));
    m_groups[QStringLiteral("waypoint")]->addToGroup(marker);
}

void MapView::wheelEvent(QWheelEvent *event)
{
    const bool follow = m_compact && m_settings.data().playerCentered;
    setTransformationAnchor(follow ? AnchorViewCenter : AnchorUnderMouse);
    const qreal factor = event->angleDelta().y() > 0 ? 1.2 : (1.0 / 1.2);
    const qreal current = transform().m11();
    if ((factor > 1.0 && current < 20.0) || (factor < 1.0 && current > 0.05)) {
        scale(factor, factor);
        updateDetailVisibility();
    }
    if (follow) {
        recenterOnPlayer();
    }
}

void MapView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    if (m_compact) {
        if (m_settings.data().playerCentered && m_state.currentPosition() != nullptr) {
            recenterOnPlayer();
        } else if (m_state.currentPosition() == nullptr) {
            fitWholeMap();
        }
        return;
    }
    // Construction-time fitInView runs while the viewport is still ~0×0, so refit
    // once the full-map view actually has a usable size.
    if (!m_didInitialFit && viewport()->width() > 40 && viewport()->height() > 40) {
        m_didInitialFit = true;
        fitWholeMap();
    }
}

void MapView::mousePressEvent(QMouseEvent *event)
{
    m_pressPos = event->pos();
    QGraphicsView::mousePressEvent(event);
}

void MapView::mouseReleaseEvent(QMouseEvent *event)
{
    const QPoint release = event->pos();
    QGraphicsView::mouseReleaseEvent(event);
    if (event->button() != Qt::LeftButton) {
        return;
    }
    if ((release - m_pressPos).manhattanLength() > 4) {
        if (m_compact && m_settings.data().playerCentered) {
            recenterOnPlayer();
        }
        return;
    }
    const QPointF scenePos = mapToScene(release);
    if (!m_scene->sceneRect().contains(scenePos)) {
        return;
    }
    if (m_state.activeWaypoint() != nullptr) {
        const QPointF marker = mapFromScene(
            m_calibration.worldToPixel(m_state.activeWaypoint()->x, m_state.activeWaypoint()->y));
        if (QLineF(release, marker).length() <= 18.0) {
            emit waypointClearRequested();
            return;
        }
    }
    const auto world = m_calibration.pixelToWorld(scenePos.x(), scenePos.y());
    emit waypointRequested(world.first, world.second);
}

} // namespace isle
