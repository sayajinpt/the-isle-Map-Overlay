#pragma once

#include "core/AppState.h"
#include "core/CoordinateTransform.h"
#include "core/LayerRepository.h"
#include "core/Settings.h"

#include <QGraphicsView>
#include <QHash>

class QGraphicsItemGroup;

namespace isle {

class MapView : public QGraphicsView {
    Q_OBJECT
public:
    MapView(Settings &settings,
            AppState &state,
            LayerRepository &repository,
            MapCalibration calibration,
            bool compact,
            QWidget *parent = nullptr);

    void reloadMapImage(const QString &path);
    void setCalibration(const MapCalibration &calibration);
    void recenterOnPlayer();
    void fitWholeMap();
    void renderStaticLayers();

signals:
    void waypointRequested(double worldX, double worldY);
    void waypointClearRequested();

public slots:
    void onPositionChanged();
    void onSettingsChanged();
    void onLayersChanged();
    void onWaypointChanged();

protected:
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    Settings &m_settings;
    AppState &m_state;
    LayerRepository &m_repository;
    MapCalibration m_calibration;
    bool m_compact = false;
    QPoint m_pressPos;
    QGraphicsScene *m_scene = nullptr;
    QGraphicsPixmapItem *m_mapItem = nullptr;
    QPixmap m_mapPixmap;
    QHash<QString, QGraphicsItemGroup *> m_groups;
    bool m_didInitialFit = false;

    void createGroups();
    void clearGroup(const QString &name);
    void ensureFollowZoom();
    void rebuildPlayerMarker();
    void rebuildBreadcrumbs();
    void rebuildWaypoint();
    void refreshVisibility();
    void updateDetailVisibility();
    QPolygonF polygonFromWorld(const QVector<QPointF> &ring) const;
    double worldRadiusToPixels(double radius) const;
    void renderZoneLayer(const QString &layer,
                         const QColor &fillDefault,
                         const QColor &outlineDefault);
    void renderPointLayer(const QString &layer, const QColor &color, bool drawLabels);
    void renderWaterOverlay();
    void renderWaterSources();
    void renderUpdrafts();
};

} // namespace isle
