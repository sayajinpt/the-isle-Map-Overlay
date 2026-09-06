#include "ui/MiniMapWindow.h"

#include "core/OverlayInteraction.h"
#include "core/Paths.h"
#include "ui/MapView.h"

#include <QCloseEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QRegion>
#include <QResizeEvent>
#include <QScreen>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace isle {

MiniMapWindow::MiniMapWindow(Settings &settings,
                             AppState &state,
                             LayerRepository &repository,
                             MapCalibration calibration,
                             QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_state(state)
    , m_repository(repository)
{
    setWindowTitle(QStringLiteral("The Isle Companion"));
    setMinimumSize(200, 240);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, false);
    setStyleSheet(QStringLiteral(
        "QToolButton { background: rgba(12,24,31,220); color: #e8fbff; border: 1px solid "
        "rgba(80,151,167,200); border-radius: 4px; padding: 2px 6px; font-size: 8pt; min-width: 22px; }"
        "QToolButton:checked { background: rgba(25,120,140,230); }"
        "QLabel#miniMapStatus { color: #d7eef3; background: rgba(12,24,31,220); border: 1px solid "
        "rgba(80,151,167,160); border-radius: 5px; padding: 2px 8px; font-size: 7.5pt; }"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(4);

    m_mapHost = new QWidget(this);
    m_mapHost->setAttribute(Qt::WA_TranslucentBackground, true);
    auto *hostLayout = new QVBoxLayout(m_mapHost);
    hostLayout->setContentsMargins(0, 0, 0, 0);
    hostLayout->setSpacing(0);

    m_mapView = new MapView(m_settings, m_state, repository, std::move(calibration), true, m_mapHost);
    m_mapView->reloadMapImage(paths::mapImagePath());
    hostLayout->addWidget(m_mapView, 1);

    m_chrome = new QWidget(m_mapHost);
    m_chrome->setObjectName(QStringLiteral("miniMapChrome"));
    m_chrome->setStyleSheet(QStringLiteral(
        "#miniMapChrome { background: rgba(12,24,31,200); border: 1px solid rgba(105,188,207,160); "
        "border-radius: 5px; }"));
    auto *chromeLayout = new QHBoxLayout(m_chrome);
    chromeLayout->setContentsMargins(5, 3, 5, 3);
    chromeLayout->setSpacing(5);
    m_optionsButton = new QToolButton(m_chrome);
    m_optionsButton->setText(QStringLiteral("Opt"));
    m_optionsButton->setToolTip(QStringLiteral("Options"));
    m_followButton = new QToolButton(m_chrome);
    m_followButton->setText(QStringLiteral("F"));
    m_followButton->setCheckable(true);
    m_followButton->setToolTip(QStringLiteral("Follow player"));
    m_shapeButton = new QToolButton(m_chrome);
    m_shapeButton->setText(QStringLiteral("◯"));
    m_shapeButton->setCheckable(true);
    m_shapeButton->setToolTip(QStringLiteral("Toggle circle / square"));
    m_quitButton = new QToolButton(m_chrome);
    m_quitButton->setText(QStringLiteral("×"));
    m_quitButton->setToolTip(QStringLiteral("Quit The Isle Companion"));
    chromeLayout->addWidget(m_optionsButton);
    chromeLayout->addWidget(m_followButton);
    chromeLayout->addWidget(m_shapeButton);
    chromeLayout->addWidget(m_quitButton);
    m_chrome->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_chrome->raise();

    root->addWidget(m_mapHost, 1);

    auto *footerRow = new QHBoxLayout();
    footerRow->setContentsMargins(8, 0, 8, 2);
    footerRow->setSpacing(0);
    footerRow->addStretch(1);
    m_statusLabel = new QLabel(QStringLiteral("Waiting…"), this);
    m_statusLabel->setObjectName(QStringLiteral("miniMapStatus"));
    m_statusLabel->setWordWrap(false);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setMinimumHeight(22);
    m_statusLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    footerRow->addWidget(m_statusLabel, 0, Qt::AlignCenter);
    footerRow->addStretch(1);
    root->addLayout(footerRow);

    connect(m_optionsButton, &QToolButton::clicked, this, &MiniMapWindow::optionsRequested);
    connect(m_followButton, &QToolButton::toggled, this, [this](bool enabled) {
        m_settings.setPlayerCentered(enabled);
        emit followToggled(enabled);
        if (enabled) {
            m_mapView->recenterOnPlayer();
        }
    });
    connect(m_shapeButton, &QToolButton::toggled, this, [this](bool circle) {
        m_settings.setOverlayShape(circle ? QStringLiteral("circle") : QStringLiteral("square"));
    });
    connect(m_quitButton, &QToolButton::clicked, this, [this]() {
        const auto answer = QMessageBox::question(
            this,
            QStringLiteral("Quit"),
            QStringLiteral("Quit The Isle Companion?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer == QMessageBox::Yes) {
            emit quitRequested();
        }
    });
    connect(&m_settings, &Settings::changed, this, &MiniMapWindow::applySettings);
    connect(&m_state, &AppState::positionChanged, this, [this](const Position &current, const Position &) {
        m_statusLabel->setText(
            QStringLiteral("%1, %2  %3")
                .arg(current.x, 0, 'f', 0)
                .arg(current.y, 0, 'f', 0)
                .arg(positionSourceName(current.source)));
        m_statusLabel->adjustSize();
        if (layout() != nullptr) {
            layout()->activate();
        }
        applyMask();
    });

    applyWindowFlags();
    applySettings();
}

void MiniMapWindow::setStatusText(const QString &text)
{
    m_statusLabel->setText(text);
    m_statusLabel->adjustSize();
    if (layout() != nullptr) {
        layout()->activate();
    }
    applyMask();
}

void MiniMapWindow::setNearestPoiText(const QString &text)
{
    if (!text.isEmpty()) {
        m_statusLabel->setToolTip(text);
    }
}

void MiniMapWindow::applyWindowFlags()
{
    Qt::WindowFlags flags = Qt::Window | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint;
    if (m_settings.data().alwaysOnTop) {
        flags |= Qt::WindowStaysOnTopHint;
    }
    const bool wasVisible = isVisible();
    setWindowFlags(flags);
    if (wasVisible) {
        show();
    }
}

void MiniMapWindow::applySettings()
{
    const auto &data = m_settings.data();
    resize(data.overlaySize, data.overlaySize + 32);
    setWindowOpacity(data.overlayOpacity);

    const bool wantOnTop = data.alwaysOnTop;
    const bool hasOnTop = windowFlags() & Qt::WindowStaysOnTopHint;
    if (wantOnTop != hasOnTop) {
        applyWindowFlags();
    }

    {
        const QSignalBlocker b1(m_followButton);
        const QSignalBlocker b2(m_shapeButton);
        m_followButton->setChecked(data.playerCentered);
        const bool circle = data.overlayShape == QStringLiteral("circle");
        m_shapeButton->setChecked(circle);
        m_shapeButton->setText(circle ? QStringLiteral("◯") : QStringLiteral("□"));
    }

    layoutOverlay();
    applyMask();
    reapplyWindowChrome();
}

void MiniMapWindow::layoutOverlay()
{
    const int side = m_settings.data().overlaySize;
    if (m_mapHost != nullptr) {
        m_mapHost->setFixedSize(side, side);
    }
    positionControls();
}

void MiniMapWindow::positionControls()
{
    if (m_mapHost == nullptr || m_chrome == nullptr) {
        return;
    }
    const int side = m_mapHost->width();
    const bool circle = m_settings.data().overlayShape == QStringLiteral("circle");
    const int top = circle ? qMax(12, int(side * 0.08)) : 6;

    if (QLayout *chromeLayout = m_chrome->layout()) {
        chromeLayout->activate();
    }
    // Size to content with a little slack so buttons never crush each other.
    const QSize tip = m_chrome->sizeHint().expandedTo(m_chrome->minimumSizeHint());
    const int chromeWidth = tip.width() + 4;
    const int chromeHeight = qMax(24, tip.height());
    const int x = qBound(4, (side - chromeWidth) / 2, qMax(4, side - chromeWidth - 4));
    m_chrome->setGeometry(x, top, chromeWidth, chromeHeight);
    m_chrome->raise();
}

void MiniMapWindow::applyMask()
{
    const int side = m_mapHost != nullptr ? m_mapHost->width() : width();
    QRegion mask;
    if (m_settings.data().overlayShape == QStringLiteral("circle")) {
        mask = QRegion(0, 0, side, side, QRegion::Ellipse);
    } else {
        mask = QRegion(0, 0, side, side, QRegion::Rectangle);
    }
    if (m_chrome != nullptr && m_chrome->isVisible()) {
        // Extra padding so the compact chrome isn't shaved by the window mask.
        const QRect chromeRect = m_chrome->geometry().adjusted(-4, -3, 4, 3);
        mask = mask.united(QRegion(chromeRect));
    }
    if (m_statusLabel != nullptr && m_statusLabel->isVisible()) {
        QRect statusRect = m_statusLabel->geometry();
        if (statusRect.width() < 8 || statusRect.height() < 8) {
            statusRect = QRect(0, side, width(), height() - side);
        }
        mask = mask.united(QRegion(statusRect.adjusted(-6, -3, 6, 3)));
    }
    setMask(mask);
    update();
}

void MiniMapWindow::saveWindowPosition()
{
    m_settings.setOverlayPosition(frameGeometry().x(), frameGeometry().y());
}

void MiniMapWindow::restoreWindowPosition()
{
    const auto &data = m_settings.data();
    if (data.overlayPosX < 0 || data.overlayPosY < 0) {
        if (QScreen *screen = QGuiApplication::primaryScreen()) {
            const QRect available = screen->availableGeometry();
            move(available.right() - width() - 16, available.bottom() - height() - 16);
        }
        return;
    }
    move(data.overlayPosX, data.overlayPosY);
}

void MiniMapWindow::reapplyWindowChrome()
{
    // Keep the window interactive and visible on the Windows taskbar.
    // Layered + capture exclusion only — no click-through / no-activate styles.
    applyWindowsOverlayInputStyle(winId(), true);
    excludeWindowFromCapture(winId());
}

void MiniMapWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_positionInitialized) {
        restoreWindowPosition();
        m_positionInitialized = true;
    }
    reapplyWindowChrome();
    // Re-measure after Qt finishes laying out the compact chrome / status chips.
    QTimer::singleShot(0, this, [this]() {
        positionControls();
        if (layout() != nullptr) {
            layout()->activate();
        }
        applyMask();
    });
}

void MiniMapWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutOverlay();
    applyMask();
}

void MiniMapWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    const QPoint pos = event->position().toPoint();
    const int side = m_mapHost != nullptr ? m_mapHost->width() : width();
    if (pos.x() > side - 28 && pos.y() > side - 28 && pos.y() < side) {
        m_resizing = true;
        m_resizeOrigin = event->globalPosition().toPoint();
        m_resizeStartSize = m_settings.data().overlaySize;
        event->accept();
        return;
    }
    if (pos.y() < 28 || pos.y() >= side) {
        m_draggingWindow = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void MiniMapWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_resizing) {
        const int delta = event->globalPosition().toPoint().x() - m_resizeOrigin.x();
        m_settings.setOverlaySize(m_resizeStartSize + delta);
        event->accept();
        return;
    }
    if (m_draggingWindow) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void MiniMapWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_draggingWindow || m_resizing) {
        m_draggingWindow = false;
        m_resizing = false;
        saveWindowPosition();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void MiniMapWindow::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    if (m_mapHost == nullptr) {
        return;
    }
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const int side = m_mapHost->width();
    const int size = 46;
    const int inset = m_settings.data().overlayShape == QStringLiteral("circle")
        ? qMax(18, int(side * 0.12))
        : 10;
    const QRect compass(inset, side - size - inset, size, size);
    painter.setBrush(QColor(12, 24, 31, 200));
    painter.setPen(QPen(QColor(80, 151, 167, 200), 1));
    painter.drawEllipse(compass);
    painter.setPen(QColor(QStringLiteral("#dff8fc")));
    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(8);
    painter.setFont(font);
    painter.drawText(compass, Qt::AlignTop | Qt::AlignHCenter, QStringLiteral("N"));
    painter.drawText(compass, Qt::AlignBottom | Qt::AlignHCenter, QStringLiteral("S"));
    painter.drawText(compass, Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral(" W"));
    painter.drawText(compass, Qt::AlignVCenter | Qt::AlignRight, QStringLiteral("E "));

    painter.setPen(QPen(QColor(105, 188, 207, 200), 2));
    painter.drawLine(side - 18, side - 8, side - 8, side - 8);
    painter.drawLine(side - 8, side - 18, side - 8, side - 8);
}

void MiniMapWindow::closeEvent(QCloseEvent *event)
{
    saveWindowPosition();
    emit quitRequested();
    event->accept();
}

} // namespace isle
