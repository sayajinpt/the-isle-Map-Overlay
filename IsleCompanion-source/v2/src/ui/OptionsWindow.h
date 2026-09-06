#pragma once

#include "core/AppState.h"
#include "core/CoordinateTransform.h"
#include "core/LayerRepository.h"
#include "core/Settings.h"

#include <QMainWindow>

class QLabel;

namespace isle {

class DataSourcePanel;
class MapView;

class OptionsWindow : public QMainWindow {
    Q_OBJECT
public:
    OptionsWindow(Settings &settings,
                  AppState &state,
                  LayerRepository &repository,
                  MapCalibration calibration,
                  const QString &projectRoot,
                  QWidget *parent = nullptr);

    MapView *mapView() const { return m_mapView; }
    void refreshPrimeSummary();
    void refreshNavLabels();
    void setCalibration(const MapCalibration &calibration);

signals:
    void clipboardToggled(bool enabled);
    void ocrToggled(bool enabled);
    void boschToggled(bool enabled);
    void ocrRegionChanged();
    void requestBoschTestPin();
    void requestOpenLiveMapScript();
    void requestChangeLiveMapServer();
    void requestOpenLiveMapPage();
    void calibrationChanged(const isle::MapCalibration &calibration);

public slots:
    void setStatusText(const QString &text);

protected:
    void showEvent(QShowEvent *event) override;

private:
    Settings &m_settings;
    AppState &m_state;
    LayerRepository &m_repository;
    MapCalibration m_calibration;
    QString m_projectRoot;
    MapView *m_mapView = nullptr;
    DataSourcePanel *m_sources = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_primeSummary = nullptr;
    QLabel *m_navLabel = nullptr;

    QWidget *buildSidePanel();
    void setupToolbar();
    void openHowTo();
    void setupOcrRegion();
    void openCalibration();
    QWidget *buildZoneFilterGroup(const QString &title,
                                  const QString &layer,
                                  const QHash<QString, bool> &values);
};

} // namespace isle
