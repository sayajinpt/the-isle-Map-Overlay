#include "core/LayerRepository.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace isle {
namespace {

const QHash<QString, QString> kLayerFiles = {
    {QStringLiteral("migrations"), QStringLiteral("migrations.json")},
    {QStringLiteral("patrol_zones"), QStringLiteral("patrol_zones.json")},
    {QStringLiteral("sanctuaries"), QStringLiteral("sanctuaries.json")},
    {QStringLiteral("updrafts"), QStringLiteral("updrafts.json")},
    {QStringLiteral("water"), QStringLiteral("water.json")},
    {QStringLiteral("locations"), QStringLiteral("locations.json")},
    {QStringLiteral("food"), QStringLiteral("food.json")},
    {QStringLiteral("ai"), QStringLiteral("ai.json")},
    {QStringLiteral("salt_licks"), QStringLiteral("salt_licks.json")},
    {QStringLiteral("spawns"), QStringLiteral("spawns.json")},
};

QVector<QPointF> parseRing(const QJsonArray &ring)
{
    QVector<QPointF> points;
    points.reserve(ring.size());
    for (const QJsonValue &value : ring) {
        const QJsonArray pair = value.toArray();
        if (pair.size() < 2) {
            continue;
        }
        points.push_back(QPointF(pair.at(0).toDouble(), pair.at(1).toDouble()));
    }
    return points;
}

} // namespace

LayerRepository::LayerRepository(QString dataDir)
    : m_dataDir(std::move(dataDir))
{
    reload();
}

void LayerRepository::reload()
{
    m_layers.clear();
    for (auto it = kLayerFiles.constBegin(); it != kLayerFiles.constEnd(); ++it) {
        const QString path = m_dataDir + QLatin1Char('/') + it.value();
        m_layers.insert(it.key(), loadFile(path));
    }
}

const QVector<LayerItem> &LayerRepository::layer(const QString &name) const
{
    const auto it = m_layers.constFind(name);
    return it == m_layers.cend() ? m_empty : it.value();
}

QStringList LayerRepository::layerNames() const
{
    return kLayerFiles.keys();
}

QVector<LayerItem> LayerRepository::loadFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const auto document = QJsonDocument::fromJson(file.readAll());
    QJsonArray items;
    if (document.isObject()) {
        items = document.object().value(QStringLiteral("items")).toArray();
    } else if (document.isArray()) {
        items = document.array();
    }
    QVector<LayerItem> result;
    result.reserve(items.size());
    for (const QJsonValue &value : items) {
        if (!value.isObject()) {
            continue;
        }
        result.push_back(parseItem(value.toObject()));
    }
    return result;
}

LayerItem LayerRepository::parseItem(const QJsonObject &object)
{
    LayerItem item;
    item.raw = object;
    item.name = object.value(QStringLiteral("name")).toString(QStringLiteral("Zone"));
    item.sourceId = object.value(QStringLiteral("source_id")).toString();
    item.color = object.value(QStringLiteral("color")).toString();
    item.description = object.value(QStringLiteral("description")).toString();
    item.showLabel = object.value(QStringLiteral("show_label")).toBool(false);
    item.radius = object.value(QStringLiteral("radius")).toDouble(0.0);

    if (object.contains(QStringLiteral("polygons"))) {
        const QJsonArray polygons = object.value(QStringLiteral("polygons")).toArray();
        for (const QJsonValue &polyValue : polygons) {
            const auto ring = parseRing(polyValue.toArray());
            if (ring.size() >= 3) {
                item.polygons.push_back(ring);
            }
        }
    } else if (object.contains(QStringLiteral("polygon"))) {
        const auto ring = parseRing(object.value(QStringLiteral("polygon")).toArray());
        if (ring.size() >= 3) {
            item.polygons.push_back(ring);
        }
    }

    if (object.contains(QStringLiteral("position"))) {
        const QJsonArray position = object.value(QStringLiteral("position")).toArray();
        if (position.size() >= 2) {
            item.position = QPointF(position.at(0).toDouble(), position.at(1).toDouble());
            item.hasPosition = true;
        }
    }
    return item;
}

} // namespace isle
