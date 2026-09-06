#pragma once

#include "core/Position.h"

#include <QObject>
#include <QTimer>

namespace isle {

class Settings;

class OcrTracker : public QObject {
    Q_OBJECT
public:
    explicit OcrTracker(Settings &settings, QObject *parent = nullptr);

    bool hasRegion() const;

public slots:
    void setEnabled(bool enabled);
    void pollOnce();

signals:
    void positionDetected(const isle::Position &position);
    void statusChanged(const QString &status);

private:
    Settings &m_settings;
    QTimer m_timer;
    bool m_enabled = false;
    QString m_lastText;

    QString runWindowsOcr() const;
};

} // namespace isle
