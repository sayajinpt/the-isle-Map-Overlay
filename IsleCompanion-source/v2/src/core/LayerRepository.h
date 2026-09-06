#pragma once

#include <QHash>
#include <QJsonObject>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>

namespace isle {

struct LayerItem {
    QString name;
    QString sourceId;
    QString color;
    QString description;
    QVector<QVector<QPointF>> polygons;
    QPointF position;
    bool hasPosition = false;
    double radius = 0.0;
    bool showLabel = false;
    QJsonObject raw;
};

class LayerRepository {
public:
    explicit LayerRepository(QString dataDir);

    void reload();
    const QVector<LayerItem> &layer(const QString &name) const;
    QStringList layerNames() const;

private:
    QString m_dataDir;
    QHash<QString, QVector<LayerItem>> m_layers;
    QVector<LayerItem> m_empty;

    static QVector<LayerItem> loadFile(const QString &path);
    static LayerItem parseItem(const QJsonObject &object);
};

} // namespace isle
