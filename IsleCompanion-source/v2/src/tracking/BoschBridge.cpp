#include "tracking/BoschBridge.h"

#include "core/Settings.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>

namespace isle {
namespace {

bool parseHttpContentLength(const QByteArray &headers, int *contentLength)
{
    if (contentLength == nullptr) {
        return false;
    }
    *contentLength = 0;
    const QByteArray lower = headers.toLower();
    const int key = lower.indexOf("content-length:");
    if (key < 0) {
        return true; // absent → treat as 0 body bytes
    }
    const int valueStart = key + 15;
    int valueEnd = headers.indexOf('\r', valueStart);
    if (valueEnd < 0) {
        valueEnd = headers.indexOf('\n', valueStart);
    }
    if (valueEnd < 0) {
        return false;
    }
    bool ok = false;
    const int length = headers.mid(valueStart, valueEnd - valueStart).trimmed().toInt(&ok);
    if (!ok || length < 0) {
        return false;
    }
    *contentLength = length;
    return true;
}

} // namespace

BoschBridge::BoschBridge(Settings &settings, MapCalibration calibration, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_calibration(std::move(calibration))
{
    connect(&m_server, &QTcpServer::newConnection, this, &BoschBridge::onNewConnection);
}

BoschBridge::~BoschBridge()
{
    stopServer();
}

bool BoschBridge::isListening() const
{
    return m_server.isListening();
}

void BoschBridge::setCalibration(const MapCalibration &calibration)
{
    m_calibration = calibration;
}

void BoschBridge::setEnabled(bool enabled)
{
    m_wanted = enabled && m_settings.data().sources.liveMap;
    if (!m_wanted) {
        stopServer();
        emit statusChanged(
            QStringLiteral("%1 bridge: off")
                .arg(liveMapProviderLabel(m_settings.data().liveMapProvider)));
        return;
    }
    const int wantedPort = m_settings.data().boschPort;
    if (isListening() && m_boundPort == wantedPort) {
        emit statusChanged(
            QStringLiteral("%1 bridge listening on 127.0.0.1:%2")
                .arg(liveMapProviderLabel(m_settings.data().liveMapProvider))
                .arg(m_boundPort));
        return;
    }
    if (!startServer()) {
        emit statusChanged(
            QStringLiteral("Live map bridge: failed to bind 127.0.0.1:%1")
                .arg(wantedPort));
        return;
    }
    emit statusChanged(
        QStringLiteral("%1 bridge listening on 127.0.0.1:%2")
            .arg(liveMapProviderLabel(m_settings.data().liveMapProvider))
            .arg(m_boundPort));
}

void BoschBridge::stopServer()
{
    m_requestBuffers.clear();
    m_server.close();
    m_boundPort = 0;
}

bool BoschBridge::startServer()
{
    stopServer();
    const int port = m_settings.data().boschPort;
    if (!m_server.listen(QHostAddress::LocalHost, quint16(port))) {
        return false;
    }
    m_boundPort = port;
    return true;
}

void BoschBridge::onNewConnection()
{
    while (m_server.hasPendingConnections()) {
        QTcpSocket *socket = m_server.nextPendingConnection();
        m_requestBuffers.insert(socket, QByteArray());
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            tryHandleBufferedRequest(socket);
        });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
            m_requestBuffers.remove(socket);
            socket->deleteLater();
        });
    }
}

bool BoschBridge::tryHandleBufferedRequest(QTcpSocket *socket)
{
    if (socket == nullptr) {
        return false;
    }
    m_requestBuffers[socket].append(socket->readAll());
    const QByteArray &request = m_requestBuffers[socket];
    const int headerEnd = request.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        return false;
    }
    const QByteArray headers = request.left(headerEnd);
    int contentLength = 0;
    if (!parseHttpContentLength(headers, &contentLength)) {
        return false;
    }
    const int totalNeeded = headerEnd + 4 + contentLength;
    if (request.size() < totalNeeded) {
        return false;
    }
    handleRequest(socket);
    m_requestBuffers.remove(socket);
    return true;
}

void BoschBridge::handleRequest(QTcpSocket *socket)
{
    if (socket == nullptr) {
        return;
    }
    const QByteArray request = m_requestBuffers.value(socket);
    const int headerEnd = request.indexOf("\r\n\r\n");
    QByteArray body;
    if (headerEnd >= 0) {
        body = request.mid(headerEnd + 4);
        int contentLength = 0;
        if (parseHttpContentLength(request.left(headerEnd), &contentLength) && contentLength >= 0) {
            body = body.left(contentLength);
        }
    }

    const bool isOptions = request.startsWith("OPTIONS ");
    const bool isPost = request.startsWith("POST /position")
        || request.startsWith("POST /bosch/position");

    QByteArray responseBody;
    int status = 404;
    if (isOptions) {
        status = 204;
    } else if (isPost) {
        bool ok = false;
        const Position position = parsePayload(body, &ok);
        if (ok) {
            status = 200;
            QJsonObject okBody;
            okBody.insert(QStringLiteral("ok"), true);
            okBody.insert(QStringLiteral("x"), position.x);
            okBody.insert(QStringLiteral("y"), position.y);
            okBody.insert(QStringLiteral("z"), position.z);
            responseBody = QJsonDocument(okBody).toJson(QJsonDocument::Compact);
            emit positionDetected(position);
        } else {
            status = 400;
            responseBody = QByteArrayLiteral("{\"ok\":false,\"error\":\"bad payload\"}");
        }
    } else if (request.startsWith("GET /health") || request.startsWith("GET / ")) {
        status = 200;
        responseBody = QByteArrayLiteral("{\"ok\":true,\"service\":\"isle-companion-v2-live-map\"}");
    }

    QByteArray response;
    response += "HTTP/1.1 " + QByteArray::number(status);
    if (status == 200) {
        response += " OK";
    } else if (status == 204) {
        response += " No Content";
    } else if (status == 400) {
        response += " Bad Request";
    } else {
        response += " Not Found";
    }
    response += "\r\n";
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    response += "Access-Control-Allow-Headers: Content-Type, Access-Control-Request-Private-Network\r\n";
    response += "Access-Control-Allow-Private-Network: true\r\n";
    response += "Connection: close\r\n";
    if (!responseBody.isEmpty()) {
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + QByteArray::number(responseBody.size()) + "\r\n";
    }
    response += "\r\n";
    response += responseBody;
    socket->write(response);
    socket->disconnectFromHost();
}

Position BoschBridge::parsePayload(const QByteArray &body, bool *ok) const
{
    Position position;
    if (ok != nullptr) {
        *ok = false;
    }
    const auto document = QJsonDocument::fromJson(body);
    if (!document.isObject()) {
        return position;
    }
    const QJsonObject object = document.object();
    const QString providerId = object.value(QStringLiteral("provider")).toString();
    PositionSource source = PositionSource::Bosch;
    if (!providerId.isEmpty()) {
        source = liveMapProviderFromId(providerId) == LiveMapProvider::VoiceIsland
            ? PositionSource::VoiceIsland
            : PositionSource::Bosch;
    } else if (m_settings.data().liveMapProvider == LiveMapProvider::VoiceIsland) {
        source = PositionSource::VoiceIsland;
    }

    if (object.contains(QStringLiteral("x")) && object.contains(QStringLiteral("y"))) {
        position.x = object.value(QStringLiteral("x")).toDouble();
        position.y = object.value(QStringLiteral("y")).toDouble();
        position.z = object.value(QStringLiteral("z")).toDouble(0.0);
        position.source = source;
        position.timestamp = QDateTime::currentDateTimeUtc();
        if (ok != nullptr) {
            *ok = true;
        }
        return position;
    }

    if (!(object.contains(QStringLiteral("map_x")) || object.contains(QStringLiteral("mapX")))) {
        return position;
    }
    const double mapX = object.value(QStringLiteral("map_x")).toDouble(
        object.value(QStringLiteral("mapX")).toDouble());
    const double mapY = object.value(QStringLiteral("map_y")).toDouble(
        object.value(QStringLiteral("mapY")).toDouble());
    const double altitude = object.value(QStringLiteral("altitude")).toDouble(
        object.value(QStringLiteral("z")).toDouble(0.0));
    const double mapSize = object.value(QStringLiteral("map_size")).toDouble(
        object.value(QStringLiteral("mapSize")).toDouble(m_settings.data().boschMapSize));
    const double size = qMax(1.0, mapSize);
    const double pixelX = (mapX / size) * (m_calibration.pixelMaxX - m_calibration.pixelMinX)
        + m_calibration.pixelMinX;
    const double pixelY = (mapY / size) * (m_calibration.pixelMaxY - m_calibration.pixelMinY)
        + m_calibration.pixelMinY;
    const auto world = m_calibration.pixelToWorld(pixelX, pixelY);
    position.x = world.first;
    position.y = world.second;
    position.z = altitude;
    position.source = source;
    position.timestamp = QDateTime::currentDateTimeUtc();
    if (ok != nullptr) {
        *ok = true;
    }
    return position;
}

} // namespace isle
