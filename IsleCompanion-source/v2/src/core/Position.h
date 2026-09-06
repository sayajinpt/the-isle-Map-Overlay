#pragma once

#include <QDateTime>
#include <QString>

namespace isle {

enum class PositionSource {
    Unknown = 0,
    Clipboard,
    Ocr,
    Bosch,
    VoiceIsland,
};

inline QString positionSourceName(PositionSource source)
{
    switch (source) {
    case PositionSource::Clipboard:
        return QStringLiteral("clipboard");
    case PositionSource::Ocr:
        return QStringLiteral("ocr");
    case PositionSource::Bosch:
        return QStringLiteral("bosch");
    case PositionSource::VoiceIsland:
        return QStringLiteral("voice-island");
    case PositionSource::Unknown:
    default:
        return QStringLiteral("unknown");
    }
}

inline bool isLiveMapSource(PositionSource source)
{
    return source == PositionSource::Bosch || source == PositionSource::VoiceIsland;
}

struct Position {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    QDateTime timestamp = QDateTime::currentDateTimeUtc();
    PositionSource source = PositionSource::Unknown;

    bool isValid() const { return timestamp.isValid(); }
};

struct Waypoint {
    QString name = QStringLiteral("Waypoint");
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

} // namespace isle
