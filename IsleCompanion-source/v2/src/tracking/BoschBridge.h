#pragma once

#include "core/CoordinateTransform.h"
#include "core/Position.h"

#include <QHash>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

namespace isle {

class Settings;

class BoschBridge : public QObject {
    Q_OBJECT
public:
    BoschBridge(Settings &settings, MapCalibration calibration, QObject *parent = nullptr);
    ~BoschBridge() override;

    bool isListening() const;

public slots:
    void setEnabled(bool enabled);
    void setCalibration(const MapCalibration &calibration);

signals:
    void positionDetected(const isle::Position &position);
    void statusChanged(const QString &status);

private slots:
    void onNewConnection();

private:
    Settings &m_settings;
    MapCalibration m_calibration;
    QTcpServer m_server;
    bool m_wanted = false;
    int m_boundPort = 0;
    QHash<QTcpSocket *, QByteArray> m_requestBuffers;

    void stopServer();
    bool startServer();
    void handleRequest(QTcpSocket *socket);
    bool tryHandleBufferedRequest(QTcpSocket *socket);
    Position parsePayload(const QByteArray &body, bool *ok) const;
};

} // namespace isle
