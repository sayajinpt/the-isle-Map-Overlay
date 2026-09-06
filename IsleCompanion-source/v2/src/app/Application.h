#pragma once

#include "core/AppState.h"
#include "core/CoordinateTransform.h"
#include "core/LayerRepository.h"
#include "core/Settings.h"
#include "tracking/BoschBridge.h"
#include "tracking/ClipboardMonitor.h"
#include "tracking/OcrTracker.h"

#include <QObject>
#include <QSystemTrayIcon>
#include <memory>

namespace isle {

class MiniMapWindow;
class OptionsWindow;

class Application : public QObject {
    Q_OBJECT
public:
    explicit Application(QObject *parent = nullptr);
    ~Application() override;

    int run();

private slots:
    void openOptions();
    void syncDataSources();
    void onStatus(const QString &status);
    void onPosition(const Position &position);
    void updateNearestPoi();
    void openLiveMapUserscript();
    void openLiveMapPage();
    void chooseLiveMapServer();
    void placeBoschTestPin();
    void onCalibrationChanged(const MapCalibration &calibration);
    void onLiveMapProviderChanged(LiveMapProvider provider);
    void toggleMiniMapVisible();

private:
    Settings m_settings;
    AppState m_state;
    LayerRepository m_repository;
    MapCalibration m_calibration;
    std::unique_ptr<ClipboardMonitor> m_clipboard;
    std::unique_ptr<BoschBridge> m_bosch;
    std::unique_ptr<OcrTracker> m_ocr;
    std::unique_ptr<MiniMapWindow> m_miniMap;
    std::unique_ptr<OptionsWindow> m_options;
    QSystemTrayIcon *m_tray = nullptr;

    void setupTray();
    bool promptLiveMapServer();
};

} // namespace isle
