#pragma once

#include <QIcon>
#include <QPainter>
#include <QPixmap>

namespace isle {

inline QIcon makeAppIcon()
{
    QPixmap pixmap(256, 256);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.setBrush(QColor(8, 22, 28));
    painter.setPen(QPen(QColor(80, 151, 167), 10));
    painter.drawEllipse(QRectF(18, 18, 220, 220));

    painter.setBrush(QColor(18, 70, 78));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QRectF(54, 54, 148, 148));

    painter.setPen(QPen(QColor(105, 188, 207), 8, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(QRectF(78, 78, 100, 100), 40 * 16, 200 * 16);

    painter.setBrush(QColor(255, 209, 102));
    painter.setPen(QPen(QColor(20, 30, 34), 4));
    painter.drawEllipse(QRectF(118, 118, 20, 20));

    painter.end();
    return QIcon(pixmap);
}

} // namespace isle
