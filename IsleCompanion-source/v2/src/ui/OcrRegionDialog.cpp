#include "ui/OcrRegionDialog.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>

namespace isle {

OcrRegionDialog::OcrRegionDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Select OCR capture area"));
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setCursor(Qt::CrossCursor);
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        setGeometry(screen->geometry());
    }
}

void OcrRegionDialog::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    m_dragging = true;
    m_origin = event->globalPosition().toPoint();
    m_region = QRect(m_origin, QSize());
    update();
}

void OcrRegionDialog::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging) {
        return;
    }
    m_region = QRect(m_origin, event->globalPosition().toPoint()).normalized();
    update();
}

void OcrRegionDialog::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_dragging || event->button() != Qt::LeftButton) {
        return;
    }
    m_dragging = false;
    m_region = QRect(m_origin, event->globalPosition().toPoint()).normalized();
    if (m_region.width() < 8 || m_region.height() < 8) {
        reject();
        return;
    }
    accept();
}

void OcrRegionDialog::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(0, 0, 0, 120));
    if (!m_region.isNull()) {
        const QRect local = QRect(
            mapFromGlobal(m_region.topLeft()),
            mapFromGlobal(m_region.bottomRight()));
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.fillRect(local, Qt::transparent);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setPen(QPen(QColor(QStringLiteral("#19D3EE")), 2));
        painter.drawRect(local);
        painter.setPen(Qt::white);
        painter.drawText(
            local.adjusted(4, 4, -4, -4),
            Qt::AlignTop | Qt::AlignLeft,
            QStringLiteral("%1 x %2").arg(m_region.width()).arg(m_region.height()));
    } else {
        painter.setPen(Qt::white);
        painter.drawText(
            rect(),
            Qt::AlignCenter,
            QStringLiteral("Drag to select Lat/Long/Alt area\nEsc to cancel"));
    }
}

void OcrRegionDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        reject();
        return;
    }
    QDialog::keyPressEvent(event);
}

} // namespace isle
