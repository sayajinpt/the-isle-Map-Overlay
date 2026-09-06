#include "tracking/ClipboardMonitor.h"

#include "core/CoordinateParser.h"
#include "core/Settings.h"

#include <QClipboard>
#include <QMimeData>

namespace isle {

ClipboardMonitor::ClipboardMonitor(Settings &settings, QClipboard *clipboard, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_clipboard(clipboard)
{
    if (m_clipboard != nullptr) {
        connect(m_clipboard, &QClipboard::dataChanged, this, &ClipboardMonitor::onClipboardChanged);
    }
}

void ClipboardMonitor::setEnabled(bool enabled)
{
    m_enabled = enabled;
    emit statusChanged(enabled ? QStringLiteral("Clipboard: on")
                               : QStringLiteral("Clipboard: off"));
}

void ClipboardMonitor::onClipboardChanged()
{
    if (!m_enabled || !m_settings.data().sources.clipboard || m_clipboard == nullptr) {
        return;
    }
    const QMimeData *mime = m_clipboard->mimeData();
    if (mime == nullptr || !mime->hasText()) {
        return;
    }
    const QString text = mime->text().trimmed();
    if (text.isEmpty() || text == m_lastText) {
        return;
    }
    const auto parsed = parseCoordinates(text);
    if (!parsed.has_value()) {
        return;
    }
    m_lastText = text;
    Position position;
    position.x = parsed->x;
    position.y = parsed->y;
    position.z = parsed->z;
    position.timestamp = QDateTime::currentDateTimeUtc();
    position.source = PositionSource::Clipboard;
    emit positionDetected(position);
    emit statusChanged(QStringLiteral("Clipboard position updated"));
}

} // namespace isle
