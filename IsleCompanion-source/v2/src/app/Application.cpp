#include "app/Application.h"

#include "core/AppIcon.h"
#include "core/Navigation.h"
#include "core/Paths.h"
#include "core/PrimeRun.h"
#include "ui/MapView.h"
#include "ui/MiniMapWindow.h"
#include "ui/OptionsWindow.h"
#include "ui/ServerChooserDialog.h"

#include <QApplication>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QMenu>
#include <QMessageBox>
#include <QUrl>

namespace isle {

Application::Application(QObject *parent)
    : QObject(parent)
    , m_state(m_settings)
    , m_repository(paths::dataDir())
{
    m_settings.load();
    m_calibration = loadCalibration(paths::calibrationPath());

    m_clipboard = std::make_unique<ClipboardMonitor>(m_settings, QGuiApplication::clipboard(), this);
    m_bosch = std::make_unique<BoschBridge>(m_settings, m_calibration, this);
    m_ocr = std::make_unique<OcrTracker>(m_settings, this);

    connect(m_clipboard.get(), &ClipboardMonitor::positionDetected, this, &Application::onPosition);
    connect(m_bosch.get(), &BoschBridge::positionDetected, this, &Application::onPosition);
    connect(m_ocr.get(), &OcrTracker::positionDetected, this, &Application::onPosition);
    connect(m_clipboard.get(), &ClipboardMonitor::statusChanged, this, &Application::onStatus);
    connect(m_bosch.get(), &BoschBridge::statusChanged, this, &Application::onStatus);
    connect(m_ocr.get(), &OcrTracker::statusChanged, this, &Application::onStatus);
    connect(&m_settings, &Settings::dataSourcesChanged, this, &Application::syncDataSources);
    connect(&m_settings, &Settings::liveMapProviderChanged, this,
            &Application::onLiveMapProviderChanged);
    connect(&m_state, &AppState::positionChanged, this, [this](const Position &, const Position &) {
        updateNearestPoi();
    });

    m_miniMap = std::make_unique<MiniMapWindow>(m_settings, m_state, m_repository, m_calibration);
    m_miniMap->setWindowIcon(makeAppIcon());
    connect(m_miniMap.get(), &MiniMapWindow::optionsRequested, this, &Application::openOptions);
    connect(m_miniMap.get(), &MiniMapWindow::quitRequested, qApp, &QApplication::quit);
    connect(m_miniMap->mapView(), &MapView::waypointRequested, this, [this](double x, double y) {
        Waypoint waypoint;
        waypoint.x = x;
        waypoint.y = y;
        m_state.setWaypoint(waypoint);
        updateNearestPoi();
    });
    connect(m_miniMap->mapView(), &MapView::waypointClearRequested, this, [this]() {
        m_state.clearWaypoint();
        updateNearestPoi();
    });

    setupTray();
}

Application::~Application() = default;

void Application::setupTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }
    const QIcon icon = makeAppIcon();
    m_tray = new QSystemTrayIcon(this);
    m_tray->setIcon(icon);
    m_tray->setToolTip(QStringLiteral("The Isle Companion v2"));
    auto *menu = new QMenu();
    menu->addAction(QStringLiteral("Show Mini Map"), this, [this]() {
        if (m_miniMap) {
            m_miniMap->show();
            m_miniMap->raise();
            m_miniMap->activateWindow();
        }
    });
    menu->addAction(QStringLiteral("Hide Mini Map"), this, [this]() {
        if (m_miniMap) {
            m_miniMap->hide();
        }
    });
    menu->addAction(QStringLiteral("Change Server…"), this, &Application::chooseLiveMapServer);
    menu->addAction(QStringLiteral("Options…"), this, &Application::openOptions);
    menu->addSeparator();
    menu->addAction(QStringLiteral("Quit"), qApp, &QApplication::quit);
    m_tray->setContextMenu(menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            toggleMiniMapVisible();
        }
    });
    m_tray->show();
}

bool Application::promptLiveMapServer()
{
    ServerChooserDialog dialog(m_settings.data().liveMapProvider);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    m_settings.setLiveMapProvider(dialog.selectedProvider());
    return true;
}

int Application::run()
{
    if (!promptLiveMapServer()) {
        return 1;
    }
    syncDataSources();
    onStatus(QStringLiteral("Using %1 live map")
                 .arg(liveMapProviderLabel(m_settings.data().liveMapProvider)));
    m_miniMap->show();
    m_miniMap->raise();
    m_miniMap->activateWindow();
    return 0;
}

void Application::chooseLiveMapServer()
{
    if (!promptLiveMapServer()) {
        return;
    }
    syncDataSources();
    if (m_options) {
        m_options->refreshPrimeSummary();
    }
    onStatus(QStringLiteral("Switched to %1")
                 .arg(liveMapProviderLabel(m_settings.data().liveMapProvider)));
}

void Application::onLiveMapProviderChanged(LiveMapProvider)
{
    m_state.clearBreadcrumbs();
    m_state.clearWaypoint();
    syncDataSources();
    if (m_options) {
        m_options->refreshPrimeSummary();
        m_options->refreshNavLabels();
    }
}

void Application::toggleMiniMapVisible()
{
    if (!m_miniMap) {
        return;
    }
    if (m_miniMap->isVisible()) {
        m_miniMap->hide();
    } else {
        m_miniMap->show();
        m_miniMap->raise();
    }
}

void Application::openOptions()
{
    if (!m_options) {
        m_options = std::make_unique<OptionsWindow>(
            m_settings, m_state, m_repository, m_calibration, paths::assetRoot());
        connect(m_options.get(), &OptionsWindow::clipboardToggled, this, [this](bool) {
            syncDataSources();
        });
        connect(m_options.get(), &OptionsWindow::ocrToggled, this, [this](bool) {
            syncDataSources();
        });
        connect(m_options.get(), &OptionsWindow::boschToggled, this, [this](bool) {
            syncDataSources();
        });
        connect(m_options.get(), &OptionsWindow::ocrRegionChanged, this, [this]() {
            syncDataSources();
        });
        connect(m_options.get(), &OptionsWindow::requestOpenLiveMapScript, this,
                &Application::openLiveMapUserscript);
        connect(m_options.get(), &OptionsWindow::requestOpenLiveMapPage, this,
                &Application::openLiveMapPage);
        connect(m_options.get(), &OptionsWindow::requestChangeLiveMapServer, this,
                &Application::chooseLiveMapServer);
        connect(m_options.get(), &OptionsWindow::requestBoschTestPin, this,
                &Application::placeBoschTestPin);
        connect(m_options.get(), &OptionsWindow::calibrationChanged, this,
                &Application::onCalibrationChanged);
    }
    m_options->setWindowIcon(makeAppIcon());
    m_options->show();
    m_options->raise();
    m_options->activateWindow();
    m_options->refreshPrimeSummary();
    m_options->refreshNavLabels();
}

void Application::syncDataSources()
{
    const auto &sources = m_settings.data().sources;
    m_clipboard->setEnabled(sources.clipboard);
    m_ocr->setEnabled(sources.ocr);
    m_bosch->setCalibration(m_calibration);
    m_bosch->setEnabled(sources.liveMap);
}

void Application::onStatus(const QString &status)
{
    if (m_miniMap) {
        m_miniMap->setStatusText(status);
    }
    if (m_options) {
        m_options->setStatusText(status);
    }
    if (m_tray) {
        m_tray->setToolTip(
            QStringLiteral("The Isle Companion v2 (%1)\n%2")
                .arg(liveMapProviderLabel(m_settings.data().liveMapProvider), status));
    }
}

void Application::updateNearestPoi()
{
    const Position *player = m_state.currentPosition();
    if (player == nullptr || !m_miniMap) {
        return;
    }
    const NearestPoi poi =
        nearestNamedPoi(*player, m_repository, m_settings.data().layers);
    if (poi.name.isEmpty()) {
        m_miniMap->setNearestPoiText(QStringLiteral("Nearest POI: —"));
        return;
    }
    m_miniMap->setNearestPoiText(
        QStringLiteral("Nearest: %1 · %2 %3")
            .arg(poi.name, formatDistance(poi.distance),
                 cardinalDirection(poi.hasHeading ? std::optional<double>(poi.heading)
                                                  : std::nullopt)));
}

void Application::onPosition(const Position &position)
{
    const auto &sources = m_settings.data().sources;
    switch (position.source) {
    case PositionSource::Clipboard:
        if (!sources.clipboard) {
            return;
        }
        break;
    case PositionSource::Ocr:
        if (!sources.ocr) {
            return;
        }
        break;
    case PositionSource::Bosch:
        if (!sources.liveMap
            || m_settings.data().liveMapProvider != LiveMapProvider::Bosch) {
            return;
        }
        break;
    case PositionSource::VoiceIsland:
        if (!sources.liveMap
            || m_settings.data().liveMapProvider != LiveMapProvider::VoiceIsland) {
            return;
        }
        break;
    case PositionSource::Unknown:
        break;
    }

    m_state.updatePosition(position);

    if (countsForPrimeRun(position, m_settings.data().liveMapProvider)) {
        const auto visits = discoverZoneVisits(m_repository, position.x, position.y);
        const auto fresh = recordVisits(m_settings, visits);
        if (!fresh.isEmpty()) {
            QStringList names;
            for (const ZoneVisit &visit : fresh) {
                names << visit.name;
            }
            onStatus(QStringLiteral("Prime run: visited %1").arg(names.join(QStringLiteral(", "))));
            if (m_options) {
                m_options->refreshPrimeSummary();
            }
        }
    }
}

void Application::openLiveMapUserscript()
{
    const LiveMapProvider provider = m_settings.data().liveMapProvider;
    const QString script = paths::assetRoot() + QStringLiteral("/tools/")
        + liveMapUserscriptFileName(provider);
    QDesktopServices::openUrl(QUrl::fromLocalFile(script));
    QMessageBox::information(
        nullptr,
        QStringLiteral("Live map userscript"),
        QStringLiteral(
            "Install/replace the Tampermonkey script for %1.\n\n"
            "1. Keep Live map bridge ON in the companion.\n"
            "2. Open the %1 livemap page and stay signed in.\n"
            "3. Confirm the badge shows the companion bridge.\n\n"
            "Voice Island: uses raw api/players coords by default (Mode cycles alternatives).\n"
            "Your companion port is %2 — scripts auto-detect 8765–8780 "
            "(you are not stuck on 8765 only).\n"
            "Bosch: script auto-reloads the tracker tab every 1m 30s.")
            .arg(liveMapProviderLabel(provider))
            .arg(m_settings.data().boschPort));
}

void Application::openLiveMapPage()
{
    QDesktopServices::openUrl(QUrl(liveMapProviderUrl(m_settings.data().liveMapProvider)));
}

void Application::placeBoschTestPin()
{
    const double mapSize = m_settings.data().boschMapSize;
    const double mapX = 374.67;
    const double mapY = 241.91;
    const double altitude = 31771.8;
    const double pixelX =
        (mapX / mapSize) * (m_calibration.pixelMaxX - m_calibration.pixelMinX)
        + m_calibration.pixelMinX;
    const double pixelY =
        (mapY / mapSize) * (m_calibration.pixelMaxY - m_calibration.pixelMinY)
        + m_calibration.pixelMinY;
    const auto world = m_calibration.pixelToWorld(pixelX, pixelY);
    Position position;
    position.x = world.first;
    position.y = world.second;
    position.z = altitude;
    position.source = m_settings.data().liveMapProvider == LiveMapProvider::VoiceIsland
        ? PositionSource::VoiceIsland
        : PositionSource::Bosch;
    position.timestamp = QDateTime::currentDateTimeUtc();
    m_settings.setLiveMapEnabled(true);
    onPosition(position);
    onStatus(QStringLiteral("%1 test pin placed")
                 .arg(liveMapProviderLabel(m_settings.data().liveMapProvider)));
}

void Application::onCalibrationChanged(const MapCalibration &calibration)
{
    m_calibration = calibration;
    m_bosch->setCalibration(calibration);
    if (m_miniMap) {
        m_miniMap->mapView()->setCalibration(calibration);
    }
    if (m_options) {
        m_options->setCalibration(calibration);
    }
}

} // namespace isle
