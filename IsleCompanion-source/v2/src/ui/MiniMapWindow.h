#pragma once

#include "core/AppState.h"
#include "core/CoordinateTransform.h"
#include "core/LayerRepository.h"
#include "core/Settings.h"

#include <QWidget>

class QLabel;
class QToolButton;

namespace isle {

class MapView;

class MiniMapWindow : public QWidget {
    Q_OBJECT
public:
    MiniMapWindow(Settings &settings,
                  AppState &state,
                  LayerRepository &repository,
                  MapCalibration calibration,
                  QWidget *parent = nullptr);

    MapView *mapView() const { return m_mapView; }
    void setStatusText(const QString &text);
    void setNearestPoiText(const QString &text);

signals:
    void optionsRequested();
    void followToggled(bool enabled);
    void quitRequested();

public slots:
    void applySettings();

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    Settings &m_settings;
    AppState &m_state;
    LayerRepository &m_repository;
    MapView *m_mapView = nullptr;
    QWidget *m_chrome = nullptr;
    QToolButton *m_optionsButton = nullptr;
    QToolButton *m_followButton = nullptr;
    QToolButton *m_shapeButton = nullptr;
    QToolButton *m_quitButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QWidget *m_mapHost = nullptr;
    bool m_draggingWindow = false;
    bool m_resizing = false;
    QPoint m_dragOffset;
    QPoint m_resizeOrigin;
    int m_resizeStartSize = 360;
    bool m_positionInitialized = false;

    void applyMask();
    void layoutOverlay();
    void positionControls();
    void saveWindowPosition();
    void restoreWindowPosition();
    void reapplyWindowChrome();
    void applyWindowFlags();
};

} // namespace isle
