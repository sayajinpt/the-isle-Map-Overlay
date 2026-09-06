#include "tracking/OcrTracker.h"

#include "core/CoordinateParser.h"
#include "core/Paths.h"
#include "core/Settings.h"

#include <QDir>
#include <QGuiApplication>
#include <QPixmap>
#include <QProcess>
#include <QScreen>
#include <QTemporaryFile>

namespace isle {

OcrTracker::OcrTracker(Settings &settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    m_timer.setInterval(500);
    connect(&m_timer, &QTimer::timeout, this, &OcrTracker::pollOnce);
}

bool OcrTracker::hasRegion() const
{
    return m_settings.data().ocrWidth > 8 && m_settings.data().ocrHeight > 8;
}

void OcrTracker::setEnabled(bool enabled)
{
    m_enabled = enabled && m_settings.data().sources.ocr;
#ifndef ISLE_COMPANION_WINDOWS
    m_enabled = false;
    emit statusChanged(QStringLiteral("OCR: Windows only"));
    m_timer.stop();
    return;
#endif
    if (!m_enabled) {
        m_timer.stop();
        emit statusChanged(QStringLiteral("OCR: off"));
        return;
    }
    if (!hasRegion()) {
        m_timer.stop();
        emit statusChanged(QStringLiteral("OCR: set capture area in Options first"));
        return;
    }
    m_timer.start();
    emit statusChanged(QStringLiteral("OCR: scanning…"));
}

QString OcrTracker::runWindowsOcr() const
{
#ifdef ISLE_COMPANION_WINDOWS
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen == nullptr || !hasRegion()) {
        return {};
    }
    const auto &d = m_settings.data();
    const QPixmap shot = screen->grabWindow(0, d.ocrX, d.ocrY, d.ocrWidth, d.ocrHeight);
    if (shot.isNull()) {
        return {};
    }
    QTemporaryFile imageFile(QDir::temp().filePath(QStringLiteral("isle-ocr-XXXXXX.png")));
    imageFile.setAutoRemove(true);
    if (!imageFile.open()) {
        return {};
    }
    shot.save(imageFile.fileName(), "PNG");

    const QString script = paths::assetRoot() + QStringLiteral("/core/windows_ocr.ps1");
    QProcess process;
    process.start(
        QStringLiteral("powershell"),
        {QStringLiteral("-NoProfile"),
         QStringLiteral("-ExecutionPolicy"),
         QStringLiteral("Bypass"),
         QStringLiteral("-File"),
         script,
         imageFile.fileName()});
    if (!process.waitForFinished(8000)) {
        process.kill();
        return {};
    }
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
#else
    return {};
#endif
}

void OcrTracker::pollOnce()
{
    if (!m_enabled || !m_settings.data().sources.ocr || !hasRegion()) {
        return;
    }
    const QString text = runWindowsOcr();
    if (text.isEmpty() || text == m_lastText) {
        return;
    }
    const auto parsed = parseCoordinatesLoose(text);
    if (!parsed.has_value()) {
        return;
    }
    m_lastText = text;
    Position position;
    position.x = parsed->x;
    position.y = parsed->y;
    position.z = parsed->z;
    position.timestamp = QDateTime::currentDateTimeUtc();
    position.source = PositionSource::Ocr;
    emit positionDetected(position);
    emit statusChanged(QStringLiteral("OCR position updated"));
}

} // namespace isle
