#include "ui/OptionsWindow.h"

#include "core/Navigation.h"
#include "core/Paths.h"
#include "core/PrimeRun.h"
#include "ui/CalibrationDialog.h"
#include "ui/DataSourcePanel.h"
#include "ui/MapView.h"
#include "ui/OcrRegionDialog.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace isle {
namespace {

struct LayerToggle {
    const char *id;
    const char *label;
};

const LayerToggle kLayerToggles[] = {
    {"migrations", "Migrations"},
    {"patrol_zones", "Patrol zones"},
    {"sanctuaries", "Sanctuaries"},
    {"updrafts", "Updrafts"},
    {"water", "Water (fresh / salt)"},
    {"locations", "Named locations"},
    {"food", "Food"},
    {"ai", "AI"},
    {"salt_licks", "Salt licks"},
    {"spawns", "Spawns"},
    {"breadcrumbs", "Breadcrumbs"},
};

} // namespace

OptionsWindow::OptionsWindow(Settings &settings,
                             AppState &state,
                             LayerRepository &repository,
                             MapCalibration calibration,
                             const QString &projectRoot,
                             QWidget *parent)
    : QMainWindow(parent)
    , m_settings(settings)
    , m_state(state)
    , m_repository(repository)
    , m_calibration(calibration)
    , m_projectRoot(projectRoot)
{
    setWindowTitle(QStringLiteral("The Isle Companion v2 — Full Map & Options"));
    resize(1460, 900);
    setupToolbar();

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    m_mapView = new MapView(m_settings, m_state, m_repository, calibration, false, splitter);
    m_mapView->reloadMapImage(paths::mapImagePath());
    auto *side = buildSidePanel();
    splitter->addWidget(m_mapView);
    splitter->addWidget(side);
    splitter->setStretchFactor(0, 1);
    splitter->setSizes({1120, 340});
    setCentralWidget(splitter);

    connect(m_mapView, &MapView::waypointRequested, this, [this](double x, double y) {
        Waypoint waypoint;
        waypoint.x = x;
        waypoint.y = y;
        m_state.setWaypoint(waypoint);
        refreshNavLabels();
    });
    connect(m_mapView, &MapView::waypointClearRequested, this, [this]() {
        m_state.clearWaypoint();
        refreshNavLabels();
    });
    connect(&m_settings, &Settings::layersChanged, this, &OptionsWindow::refreshPrimeSummary);
    connect(&m_state, &AppState::positionChanged, this, [this](const Position &, const Position &) {
        refreshPrimeSummary();
        refreshNavLabels();
    });
    connect(&m_state, &AppState::waypointChanged, this, &OptionsWindow::refreshNavLabels);
    refreshPrimeSummary();
    refreshNavLabels();
}

void OptionsWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    // Fit after the window/layout has a real size (constructor fit is too early).
    QTimer::singleShot(0, this, [this]() {
        if (m_mapView != nullptr) {
            m_mapView->fitWholeMap();
        }
    });
}

void OptionsWindow::setupToolbar()
{
    auto *toolbar = addToolBar(QStringLiteral("Map"));
    toolbar->setMovable(false);
    toolbar->addAction(QStringLiteral("Fit"), this, [this]() {
        if (m_mapView) {
            m_mapView->fitWholeMap();
        }
    });
    toolbar->addAction(QStringLiteral("Center Player"), this, [this]() {
        if (m_mapView) {
            m_mapView->recenterOnPlayer();
        }
    });
    toolbar->addAction(QStringLiteral("Clear Trail"), this, [this]() {
        m_state.clearBreadcrumbs();
    });
    toolbar->addAction(QStringLiteral("Clear Waypoint"), this, [this]() {
        m_state.clearWaypoint();
    });
    toolbar->addSeparator();
    toolbar->addAction(QStringLiteral("Set OCR Area…"), this, [this]() { setupOcrRegion(); });
    toolbar->addAction(QStringLiteral("Change Server…"), this, [this]() {
        emit requestChangeLiveMapServer();
    });
    toolbar->addAction(QStringLiteral("Live Map Userscript…"), this, [this]() {
        emit requestOpenLiveMapScript();
    });
    toolbar->addAction(QStringLiteral("Open Live Map…"), this, [this]() {
        emit requestOpenLiveMapPage();
    });
    toolbar->addAction(QStringLiteral("Live Map Test Pin"), this, [this]() {
        emit requestBoschTestPin();
    });
    toolbar->addAction(QStringLiteral("Calibration…"), this, [this]() { openCalibration(); });
    toolbar->addSeparator();
    toolbar->addAction(QStringLiteral("How to use"), this, [this]() { openHowTo(); });
}

QWidget *OptionsWindow::buildSidePanel()
{
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setMinimumWidth(310);
    scroll->setMaximumWidth(400);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *panel = new QWidget(scroll);
    auto *layout = new QVBoxLayout(panel);

    auto *title = new QLabel(QStringLiteral("Options"), panel);
    title->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: 700;"));
    layout->addWidget(title);

    m_navLabel = new QLabel(QStringLiteral("Nearest POI: —"), panel);
    m_navLabel->setWordWrap(true);
    layout->addWidget(m_navLabel);

    m_sources = new DataSourcePanel(m_settings, panel);
    layout->addWidget(m_sources);
    connect(m_sources, &DataSourcePanel::clipboardToggled, this, [this](bool enabled) {
        m_settings.setClipboardEnabled(enabled);
        emit clipboardToggled(enabled);
    });
    connect(m_sources, &DataSourcePanel::ocrToggled, this, [this](bool enabled) {
        m_settings.setOcrEnabled(enabled);
        emit ocrToggled(enabled);
    });
    connect(m_sources, &DataSourcePanel::boschToggled, this, [this](bool enabled) {
        m_settings.setBoschEnabled(enabled);
        emit boschToggled(enabled);
    });

    auto *miniGroup = new QGroupBox(QStringLiteral("Mini Map"), panel);
    auto *miniLayout = new QVBoxLayout(miniGroup);
    auto *follow = new QCheckBox(QStringLiteral("Follow / center on player"), miniGroup);
    follow->setChecked(m_settings.data().playerCentered);
    auto *onTop = new QCheckBox(QStringLiteral("Always on top"), miniGroup);
    onTop->setChecked(m_settings.data().alwaysOnTop);
    auto *circle = new QCheckBox(QStringLiteral("Circular Mini Map"), miniGroup);
    circle->setChecked(m_settings.data().overlayShape == QStringLiteral("circle"));
    auto *labels = new QCheckBox(QStringLiteral("Show POI labels on Mini Map"), miniGroup);
    labels->setChecked(m_settings.data().miniMapShowPoiLabels);
    auto *crumbLines = new QCheckBox(QStringLiteral("Connect breadcrumb trail"), miniGroup);
    crumbLines->setChecked(m_settings.data().breadcrumbConnectLines);
    miniLayout->addWidget(follow);
    miniLayout->addWidget(onTop);
    miniLayout->addWidget(circle);
    miniLayout->addWidget(labels);
    miniLayout->addWidget(crumbLines);

    auto *sizeLabel = new QLabel(QStringLiteral("Size"), miniGroup);
    auto *size = new QSlider(Qt::Horizontal, miniGroup);
    size->setRange(180, 900);
    size->setValue(m_settings.data().overlaySize);
    auto *opacityLabel = new QLabel(QStringLiteral("Opacity"), miniGroup);
    auto *opacity = new QSlider(Qt::Horizontal, miniGroup);
    opacity->setRange(20, 100);
    opacity->setValue(int(m_settings.data().overlayOpacity * 100));
    miniLayout->addWidget(sizeLabel);
    miniLayout->addWidget(size);
    miniLayout->addWidget(opacityLabel);
    miniLayout->addWidget(opacity);
    layout->addWidget(miniGroup);

    connect(follow, &QCheckBox::toggled, &m_settings, &Settings::setPlayerCentered);
    connect(onTop, &QCheckBox::toggled, &m_settings, &Settings::setAlwaysOnTop);
    connect(circle, &QCheckBox::toggled, this, [this](bool enabled) {
        m_settings.setOverlayShape(enabled ? QStringLiteral("circle") : QStringLiteral("square"));
    });
    connect(labels, &QCheckBox::toggled, &m_settings, &Settings::setMiniMapShowPoiLabels);
    connect(crumbLines, &QCheckBox::toggled, &m_settings, &Settings::setBreadcrumbConnectLines);
    connect(size, &QSlider::valueChanged, &m_settings, &Settings::setOverlaySize);
    connect(opacity, &QSlider::valueChanged, this, [this](int value) {
        m_settings.setOverlayOpacity(value / 100.0);
    });

    auto *boschGroup = new QGroupBox(QStringLiteral("Live map bridge"), panel);
    auto *boschLayout = new QVBoxLayout(boschGroup);
    auto *serverLabel = new QLabel(
        QStringLiteral("Active server: %1")
            .arg(liveMapProviderLabel(m_settings.data().liveMapProvider)),
        boschGroup);
    serverLabel->setWordWrap(true);
    auto *port = new QSpinBox(boschGroup);
    port->setRange(1024, 65535);
    port->setValue(m_settings.data().boschPort);
    auto *mapSize = new QDoubleSpinBox(boschGroup);
    mapSize->setRange(100, 4000);
    mapSize->setValue(m_settings.data().boschMapSize);
    boschLayout->addWidget(serverLabel);
    boschLayout->addWidget(new QLabel(
        QStringLiteral("Port (userscripts auto-detect 8765–8780; yours is currently set below)"),
        boschGroup));
    boschLayout->addWidget(port);
    boschLayout->addWidget(new QLabel(QStringLiteral("Tracker map size"), boschGroup));
    boschLayout->addWidget(mapSize);
    layout->addWidget(boschGroup);
    connect(&m_settings, &Settings::liveMapProviderChanged, this, [serverLabel](LiveMapProvider provider) {
        serverLabel->setText(
            QStringLiteral("Active server: %1").arg(liveMapProviderLabel(provider)));
    });
    connect(port, &QSpinBox::valueChanged, this, [this](int value) {
        m_settings.data().boschPort = value;
        m_settings.save();
        emit boschToggled(m_settings.data().sources.liveMap);
    });
    connect(mapSize, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        m_settings.setBridgeMapSize(value);
    });

    auto *layersGroup = new QGroupBox(QStringLiteral("Map layers"), panel);
    auto *layersLayout = new QVBoxLayout(layersGroup);
    for (const LayerToggle &toggle : kLayerToggles) {
        auto *box = new QCheckBox(QString::fromUtf8(toggle.label), layersGroup);
        const QString id = QString::fromUtf8(toggle.id);
        box->setChecked(m_settings.data().layers.value(id, true));
        connect(box, &QCheckBox::toggled, this, [this, id](bool enabled) {
            m_settings.setLayerEnabled(id, enabled);
        });
        layersLayout->addWidget(box);
    }
    layout->addWidget(layersGroup);

    auto *filters = new QGroupBox(QStringLiteral("Individual zone filters"), panel);
    auto *filtersLayout = new QVBoxLayout(filters);
    auto *toggleFilters = new QToolButton(filters);
    toggleFilters->setText(QStringLiteral("Show / hide zone checklists"));
    toggleFilters->setCheckable(true);
    auto *filterBody = new QWidget(filters);
    filterBody->setVisible(false);
    auto *filterBodyLayout = new QVBoxLayout(filterBody);
    filterBodyLayout->setContentsMargins(0, 0, 0, 0);
    filterBodyLayout->addWidget(buildZoneFilterGroup(
        QStringLiteral("Migrations"),
        QStringLiteral("migrations"),
        m_settings.data().individualMigrations));
    filterBodyLayout->addWidget(buildZoneFilterGroup(
        QStringLiteral("Patrol zones"),
        QStringLiteral("patrol_zones"),
        m_settings.data().individualPatrols));
    filterBodyLayout->addWidget(buildZoneFilterGroup(
        QStringLiteral("Sanctuaries"),
        QStringLiteral("sanctuaries"),
        m_settings.data().individualSanctuaries));
    filtersLayout->addWidget(toggleFilters);
    filtersLayout->addWidget(filterBody);
    connect(toggleFilters, &QToolButton::toggled, filterBody, &QWidget::setVisible);
    layout->addWidget(filters);

    auto *primeGroup = new QGroupBox(QStringLiteral("Prime run"), panel);
    auto *primeLayout = new QVBoxLayout(primeGroup);
    auto *primeHint = new QLabel(
        QStringLiteral(
            "Live-map credit for the active server only (Bosch or Voice Island). "
            "Goals: 1 sanctuary, 2 migrations, 4 patrols. Progress is saved per server."),
        primeGroup);
    primeHint->setWordWrap(true);
    m_primeSummary = new QLabel(primeGroup);
    m_primeSummary->setWordWrap(true);
    auto *clearPrime = new QPushButton(QStringLiteral("Clear prime run…"), primeGroup);
    primeLayout->addWidget(primeHint);
    primeLayout->addWidget(m_primeSummary);
    primeLayout->addWidget(clearPrime);
    layout->addWidget(primeGroup);
    connect(clearPrime, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::question(
                this,
                QStringLiteral("Clear prime run"),
                QStringLiteral("Clear visited zones for the active server (%1)?")
                    .arg(liveMapProviderLabel(m_settings.data().liveMapProvider)))
            == QMessageBox::Yes) {
            clearPrimeRun(m_settings);
            refreshPrimeSummary();
        }
    });

    m_status = new QLabel(QStringLiteral("Ready"), panel);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    layout->addStretch(1);
    scroll->setWidget(panel);
    return scroll;
}

void OptionsWindow::setStatusText(const QString &text)
{
    if (m_status) {
        m_status->setText(text);
    }
}

void OptionsWindow::refreshPrimeSummary()
{
    if (m_primeSummary) {
        m_primeSummary->setText(primeRunProgress(m_repository, m_settings).summaryLines().join('\n'));
    }
}

void OptionsWindow::refreshNavLabels()
{
    if (!m_navLabel) {
        return;
    }
    const Position *player = m_state.currentPosition();
    if (player == nullptr) {
        m_navLabel->setText(QStringLiteral("Nearest POI: waiting for position"));
        return;
    }
    const NearestPoi poi =
        nearestNamedPoi(*player, m_repository, m_settings.data().layers);
    QString text;
    if (poi.name.isEmpty()) {
        text = QStringLiteral("Nearest POI: —");
    } else {
        text = QStringLiteral("Nearest: %1 · %2 %3")
                   .arg(poi.name, formatDistance(poi.distance),
                        cardinalDirection(poi.hasHeading ? std::optional<double>(poi.heading)
                                                         : std::nullopt));
    }
    if (const Waypoint *waypoint = m_state.activeWaypoint()) {
        const double distance = planarDistance(player->x, player->y, waypoint->x, waypoint->y);
        const auto heading = headingDegrees(player->x, player->y, waypoint->x, waypoint->y);
        text += QStringLiteral("\nWaypoint: %1 %2")
                    .arg(formatDistance(distance), cardinalDirection(heading));
    }
    m_navLabel->setText(text);
}

void OptionsWindow::setupOcrRegion()
{
    OcrRegionDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QRect region = dialog.selectedRegion();
    m_settings.setOcrRegion(region.x(), region.y(), region.width(), region.height());
    emit ocrRegionChanged();
    setStatusText(QStringLiteral("OCR area saved (%1x%2)")
                      .arg(region.width())
                      .arg(region.height()));
}

void OptionsWindow::openCalibration()
{
    CalibrationDialog dialog(m_calibration, paths::calibrationPath(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    m_calibration = dialog.calibration();
    if (m_mapView) {
        m_mapView->setCalibration(m_calibration);
    }
    emit calibrationChanged(m_calibration);
    setStatusText(QStringLiteral("Calibration saved"));
}

void OptionsWindow::setCalibration(const MapCalibration &calibration)
{
    m_calibration = calibration;
    if (m_mapView) {
        m_mapView->setCalibration(calibration);
    }
}

QWidget *OptionsWindow::buildZoneFilterGroup(const QString &title,
                                             const QString &layer,
                                             const QHash<QString, bool> &values)
{
    auto *group = new QGroupBox(title);
    auto *layout = new QVBoxLayout(group);
    for (const LayerItem &zone : m_repository.layer(layer)) {
        auto *box = new QCheckBox(zone.name, group);
        box->setChecked(values.value(zone.name, true));
        connect(box, &QCheckBox::toggled, this, [this, layer, name = zone.name](bool enabled) {
            m_settings.setIndividualZoneEnabled(layer, name, enabled);
        });
        layout->addWidget(box);
    }
    if (layout->count() == 0) {
        layout->addWidget(new QLabel(QStringLiteral("No zones loaded"), group));
    }
    return group;
}

void OptionsWindow::openHowTo()
{
    QMessageBox::information(
        this,
        QStringLiteral("How to use — v2"),
        QStringLiteral(
            "Mini Map is the main window.\n\n"
            "• Options opens this full map + settings.\n"
            "• Toggle Clipboard / OCR / Live map bridge as data sources.\n"
            "• Choose Bosch or Voice Island at launch (also via Change Server…).\n"
            "• Prime-run visits and per-server filters are saved separately for each server.\n"
            "• Mini Map stays interactive — drag top/footer to move; corner to resize.\n"
            "• Click the full map to place a waypoint; click it again to clear.\n"
            "• Live map: install the matching Tampermonkey script from Options.\n"
            "  Bosch: tools/bosch_island_bridge.user.js\n"
            "  Voice Island: tools/voice_island_bridge.user.js\n"
            "• Voice Island: reads api/players (~5s). Set your map name in the badge if many players.\n"
            "• Voice Island livemap: https://voice-island.com/dashboard/livemap\n"
            "• OCR: Set OCR Area, enable OCR source, hold Tab in-game.\n"
            "• Use the taskbar icon or × to quit."));
}

} // namespace isle
