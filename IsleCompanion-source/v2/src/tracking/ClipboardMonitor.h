#pragma once

#include "core/Position.h"

#include <QObject>

class QClipboard;

namespace isle {

class Settings;

class ClipboardMonitor : public QObject {
    Q_OBJECT
public:
    ClipboardMonitor(Settings &settings, QClipboard *clipboard, QObject *parent = nullptr);

public slots:
    void setEnabled(bool enabled);

signals:
    void positionDetected(const isle::Position &position);
    void statusChanged(const QString &status);

private slots:
    void onClipboardChanged();

private:
    Settings &m_settings;
    QClipboard *m_clipboard = nullptr;
    bool m_enabled = false;
    QString m_lastText;
};

} // namespace isle
