#include "core/Settings.h"

#include "core/Paths.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtGlobal>

namespace isle {
namespace {

QHash<QString, bool> loadBoolHash(const QJsonObject &object)
{
    QHash<QString, bool> out;
    for (auto it = object.begin(); it != object.end(); ++it) {
        out.insert(it.key(), it.value().toBool(true));
    }
    return out;
}

QJsonObject dumpBoolHash(const QHash<QString, bool> &values)
{
    QJsonObject object;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        object.insert(it.key(), it.value());
    }
    return object;
}

QSet<QString> loadStringSet(const QJsonArray &array)
{
    QSet<QString> out;
    for (const QJsonValue &value : array) {
        const QString id = value.toString().trimmed();
        if (!id.isEmpty()) {
            out.insert(id);
        }
    }
    return out;
}

QJsonArray dumpStringSet(const QSet<QString> &values)
{
    QJsonArray array;
    for (const QString &id : values) {
        array.append(id);
    }
    return array;
}

} // namespace

Settings::Settings(QObject *parent)
    : QObject(parent)
    , m_data(defaults())
    , m_path(paths::settingsPath())
{
}

SettingsData Settings::defaults()
{
    SettingsData data;
    data.profiles.insert(liveMapProviderId(LiveMapProvider::Bosch), ServerProfileData{});
    data.profiles.insert(liveMapProviderId(LiveMapProvider::VoiceIsland), ServerProfileData{});
    return data;
}

ServerProfileData Settings::profileFromActive(const SettingsData &data)
{
    ServerProfileData profile;
    profile.liveMapEnabled = data.sources.liveMap;
    profile.bridgeMapSize = data.boschMapSize;
    profile.primeRunActive = data.primeRunActive;
    profile.visitedZoneIds = data.visitedZoneIds;
    profile.individualMigrations = data.individualMigrations;
    profile.individualPatrols = data.individualPatrols;
    profile.individualSanctuaries = data.individualSanctuaries;
    return profile;
}

void Settings::applyProfile(SettingsData *data, const ServerProfileData &profile)
{
    if (data == nullptr) {
        return;
    }
    data->sources.liveMap = profile.liveMapEnabled;
    data->boschMapSize = profile.bridgeMapSize;
    data->primeRunActive = profile.primeRunActive;
    data->visitedZoneIds = profile.visitedZoneIds;
    data->individualMigrations = profile.individualMigrations;
    data->individualPatrols = profile.individualPatrols;
    data->individualSanctuaries = profile.individualSanctuaries;
}

void Settings::stashActiveProfile(SettingsData *data)
{
    if (data == nullptr) {
        return;
    }
    data->profiles.insert(liveMapProviderId(data->liveMapProvider), profileFromActive(*data));
}

ServerProfileData Settings::profileFromJson(const QJsonObject &object)
{
    ServerProfileData profile;
    profile.liveMapEnabled = object.value(QStringLiteral("live_map_enabled"))
                                 .toBool(object.value(QStringLiteral("bosch")).toBool(false));
    profile.bridgeMapSize = object.value(QStringLiteral("bridge_map_size"))
                                .toDouble(object.value(QStringLiteral("bosch_map_size")).toDouble(750.0));
    profile.primeRunActive = object.value(QStringLiteral("prime_run_active")).toBool(true);
    profile.visitedZoneIds = loadStringSet(object.value(QStringLiteral("visited_zone_ids")).toArray());
    profile.individualMigrations =
        loadBoolHash(object.value(QStringLiteral("individual_migrations")).toObject());
    profile.individualPatrols =
        loadBoolHash(object.value(QStringLiteral("individual_patrol_zones")).toObject());
    profile.individualSanctuaries =
        loadBoolHash(object.value(QStringLiteral("individual_sanctuaries")).toObject());
    return profile;
}

QJsonObject Settings::profileToJson(const ServerProfileData &profile)
{
    QJsonObject object;
    object.insert(QStringLiteral("live_map_enabled"), profile.liveMapEnabled);
    object.insert(QStringLiteral("bridge_map_size"), profile.bridgeMapSize);
    object.insert(QStringLiteral("prime_run_active"), profile.primeRunActive);
    object.insert(QStringLiteral("visited_zone_ids"), dumpStringSet(profile.visitedZoneIds));
    object.insert(QStringLiteral("individual_migrations"),
                  dumpBoolHash(profile.individualMigrations));
    object.insert(QStringLiteral("individual_patrol_zones"),
                  dumpBoolHash(profile.individualPatrols));
    object.insert(QStringLiteral("individual_sanctuaries"),
                  dumpBoolHash(profile.individualSanctuaries));
    return object;
}

void Settings::load()
{
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_data = defaults();
        return;
    }
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        m_data = defaults();
        return;
    }
    m_data = fromJson(document.object());
    emit changed();
    emit dataSourcesChanged();
    emit layersChanged();
    emit liveMapProviderChanged(m_data.liveMapProvider);
}

void Settings::save() const
{
    SettingsData data = m_data;
    stashActiveProfile(&data);
    QFile file(m_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(QJsonDocument(toJson(data)).toJson(QJsonDocument::Indented));
}

SettingsData Settings::fromJson(const QJsonObject &object)
{
    SettingsData data = defaults();
    const auto sources = object.value(QStringLiteral("sources")).toObject();
    data.sources.clipboard = sources.value(QStringLiteral("clipboard")).toBool(true);
    data.sources.ocr = sources.value(QStringLiteral("ocr")).toBool(false);
    data.sources.liveMap = sources.value(QStringLiteral("live_map"))
                               .toBool(sources.value(QStringLiteral("bosch")).toBool(false));
    data.alwaysOnTop = object.value(QStringLiteral("always_on_top")).toBool(true);
    data.playerCentered = object.value(QStringLiteral("player_centered")).toBool(true);
    data.overlayOpacity = object.value(QStringLiteral("overlay_opacity")).toDouble(0.92);
    data.overlaySize = object.value(QStringLiteral("overlay_size")).toInt(360);
    data.overlayShape = object.value(QStringLiteral("overlay_shape")).toString(QStringLiteral("circle"));
    data.boschPort = object.value(QStringLiteral("bosch_port")).toInt(8765);
    data.boschMapSize = object.value(QStringLiteral("bosch_map_size")).toDouble(750.0);
    data.breadcrumbsEnabled = object.value(QStringLiteral("breadcrumbs_enabled")).toBool(true);
    data.breadcrumbConnectLines =
        object.value(QStringLiteral("breadcrumb_connect_lines")).toBool(true);
    data.breadcrumbMaxPoints = object.value(QStringLiteral("breadcrumb_max_points")).toInt(500);
    data.primeRunActive = object.value(QStringLiteral("prime_run_active")).toBool(true);
    data.miniMapShowPoiLabels =
        object.value(QStringLiteral("mini_map_show_poi_labels")).toBool(false);
    data.interactiveByDefault =
        object.value(QStringLiteral("interactive_by_default")).toBool(true);
    data.overlayPosX = object.value(QStringLiteral("overlay_pos_x")).toInt(-1);
    data.overlayPosY = object.value(QStringLiteral("overlay_pos_y")).toInt(-1);
    const auto ocr = object.value(QStringLiteral("ocr_region")).toObject();
    data.ocrX = ocr.value(QStringLiteral("x")).toInt(0);
    data.ocrY = ocr.value(QStringLiteral("y")).toInt(0);
    data.ocrWidth = ocr.value(QStringLiteral("width")).toInt(0);
    data.ocrHeight = ocr.value(QStringLiteral("height")).toInt(0);
    data.interactionBinding =
        object.value(QStringLiteral("interaction_binding")).toString(QStringLiteral("M4"));
    data.overlaySize = qBound(180, data.overlaySize, 1200);
    data.overlayOpacity = qBound(0.2, data.overlayOpacity, 1.0);
    data.boschPort = qBound(1024, data.boschPort, 65535);
    if (data.overlayShape != QStringLiteral("circle")
        && data.overlayShape != QStringLiteral("square")) {
        data.overlayShape = QStringLiteral("circle");
    }

    data.liveMapProvider = liveMapProviderFromId(
        object.value(QStringLiteral("live_map_provider")).toString(QStringLiteral("bosch")));

    const QJsonObject layers = object.value(QStringLiteral("layers")).toObject();
    for (auto it = layers.begin(); it != layers.end(); ++it) {
        data.layers.insert(it.key(), it.value().toBool(true));
    }

    data.visitedZoneIds = loadStringSet(object.value(QStringLiteral("visited_zone_ids")).toArray());
    data.individualMigrations =
        loadBoolHash(object.value(QStringLiteral("individual_migrations")).toObject());
    data.individualPatrols =
        loadBoolHash(object.value(QStringLiteral("individual_patrol_zones")).toObject());
    data.individualSanctuaries =
        loadBoolHash(object.value(QStringLiteral("individual_sanctuaries")).toObject());

    const QJsonObject profilesObject = object.value(QStringLiteral("server_profiles")).toObject();
    if (!profilesObject.isEmpty()) {
        for (auto it = profilesObject.begin(); it != profilesObject.end(); ++it) {
            if (!it.value().isObject()) {
                continue;
            }
            data.profiles.insert(it.key(), profileFromJson(it.value().toObject()));
        }
    } else {
        // Migrate flat legacy settings into the Bosch profile.
        ServerProfileData bosch = profileFromActive(data);
        bosch.liveMapEnabled = data.sources.liveMap;
        data.profiles.insert(liveMapProviderId(LiveMapProvider::Bosch), bosch);
        if (!data.profiles.contains(liveMapProviderId(LiveMapProvider::VoiceIsland))) {
            data.profiles.insert(liveMapProviderId(LiveMapProvider::VoiceIsland),
                                 ServerProfileData{});
        }
    }

    const QString activeId = liveMapProviderId(data.liveMapProvider);
    if (data.profiles.contains(activeId)) {
        applyProfile(&data, data.profiles.value(activeId));
    } else {
        data.profiles.insert(activeId, profileFromActive(data));
    }
    return data;
}

QJsonObject Settings::toJson(const SettingsData &data)
{
    SettingsData writable = data;
    stashActiveProfile(&writable);

    QJsonObject sources;
    sources.insert(QStringLiteral("clipboard"), writable.sources.clipboard);
    sources.insert(QStringLiteral("ocr"), writable.sources.ocr);
    sources.insert(QStringLiteral("live_map"), writable.sources.liveMap);
    sources.insert(QStringLiteral("bosch"), writable.sources.liveMap); // legacy alias

    QJsonObject layers;
    for (auto it = writable.layers.constBegin(); it != writable.layers.constEnd(); ++it) {
        layers.insert(it.key(), it.value());
    }

    QJsonObject profiles;
    for (auto it = writable.profiles.constBegin(); it != writable.profiles.constEnd(); ++it) {
        profiles.insert(it.key(), profileToJson(it.value()));
    }

    QJsonObject object;
    object.insert(QStringLiteral("live_map_provider"), liveMapProviderId(writable.liveMapProvider));
    object.insert(QStringLiteral("server_profiles"), profiles);
    object.insert(QStringLiteral("sources"), sources);
    object.insert(QStringLiteral("layers"), layers);
    object.insert(QStringLiteral("visited_zone_ids"), dumpStringSet(writable.visitedZoneIds));
    object.insert(QStringLiteral("always_on_top"), writable.alwaysOnTop);
    object.insert(QStringLiteral("player_centered"), writable.playerCentered);
    object.insert(QStringLiteral("overlay_opacity"), writable.overlayOpacity);
    object.insert(QStringLiteral("overlay_size"), writable.overlaySize);
    object.insert(QStringLiteral("overlay_shape"), writable.overlayShape);
    object.insert(QStringLiteral("bosch_port"), writable.boschPort);
    object.insert(QStringLiteral("bosch_map_size"), writable.boschMapSize);
    object.insert(QStringLiteral("breadcrumbs_enabled"), writable.breadcrumbsEnabled);
    object.insert(QStringLiteral("breadcrumb_connect_lines"), writable.breadcrumbConnectLines);
    object.insert(QStringLiteral("breadcrumb_max_points"), writable.breadcrumbMaxPoints);
    object.insert(QStringLiteral("prime_run_active"), writable.primeRunActive);
    object.insert(QStringLiteral("mini_map_show_poi_labels"), writable.miniMapShowPoiLabels);
    object.insert(QStringLiteral("interactive_by_default"), writable.interactiveByDefault);
    object.insert(QStringLiteral("overlay_pos_x"), writable.overlayPosX);
    object.insert(QStringLiteral("overlay_pos_y"), writable.overlayPosY);
    QJsonObject ocr;
    ocr.insert(QStringLiteral("x"), writable.ocrX);
    ocr.insert(QStringLiteral("y"), writable.ocrY);
    ocr.insert(QStringLiteral("width"), writable.ocrWidth);
    ocr.insert(QStringLiteral("height"), writable.ocrHeight);
    object.insert(QStringLiteral("ocr_region"), ocr);
    object.insert(QStringLiteral("interaction_binding"), writable.interactionBinding);
    object.insert(QStringLiteral("individual_migrations"),
                  dumpBoolHash(writable.individualMigrations));
    object.insert(QStringLiteral("individual_patrol_zones"),
                  dumpBoolHash(writable.individualPatrols));
    object.insert(QStringLiteral("individual_sanctuaries"),
                  dumpBoolHash(writable.individualSanctuaries));
    return object;
}

void Settings::setClipboardEnabled(bool enabled)
{
    if (m_data.sources.clipboard == enabled) {
        return;
    }
    m_data.sources.clipboard = enabled;
    save();
    emit dataSourcesChanged();
    emit changed();
}

void Settings::setOcrEnabled(bool enabled)
{
    if (m_data.sources.ocr == enabled) {
        return;
    }
    m_data.sources.ocr = enabled;
    save();
    emit dataSourcesChanged();
    emit changed();
}

void Settings::setLiveMapEnabled(bool enabled)
{
    if (m_data.sources.liveMap == enabled) {
        return;
    }
    m_data.sources.liveMap = enabled;
    save();
    emit dataSourcesChanged();
    emit changed();
}

void Settings::setBoschEnabled(bool enabled)
{
    setLiveMapEnabled(enabled);
}

void Settings::setLiveMapProvider(LiveMapProvider provider)
{
    if (m_data.liveMapProvider == provider) {
        return;
    }
    stashActiveProfile(&m_data);
    m_data.liveMapProvider = provider;
    const QString id = liveMapProviderId(provider);
    if (!m_data.profiles.contains(id)) {
        m_data.profiles.insert(id, ServerProfileData{});
    }
    applyProfile(&m_data, m_data.profiles.value(id));
    save();
    emit liveMapProviderChanged(provider);
    emit dataSourcesChanged();
    emit layersChanged();
    emit changed();
}

void Settings::setPlayerCentered(bool enabled)
{
    if (m_data.playerCentered == enabled) {
        return;
    }
    m_data.playerCentered = enabled;
    save();
    emit changed();
}

void Settings::setAlwaysOnTop(bool enabled)
{
    if (m_data.alwaysOnTop == enabled) {
        return;
    }
    m_data.alwaysOnTop = enabled;
    save();
    emit changed();
}

void Settings::setOverlaySize(int size)
{
    size = qBound(180, size, 1200);
    if (m_data.overlaySize == size) {
        return;
    }
    m_data.overlaySize = size;
    save();
    emit changed();
}

void Settings::setOverlayOpacity(double opacity)
{
    opacity = qBound(0.2, opacity, 1.0);
    if (qFuzzyCompare(m_data.overlayOpacity, opacity)) {
        return;
    }
    m_data.overlayOpacity = opacity;
    save();
    emit changed();
}

void Settings::setOverlayShape(const QString &shape)
{
    const QString normalized =
        shape == QStringLiteral("square") ? QStringLiteral("square") : QStringLiteral("circle");
    if (m_data.overlayShape == normalized) {
        return;
    }
    m_data.overlayShape = normalized;
    save();
    emit changed();
}

void Settings::setLayerEnabled(const QString &layer, bool enabled)
{
    if (m_data.layers.value(layer, true) == enabled) {
        return;
    }
    m_data.layers.insert(layer, enabled);
    save();
    emit layersChanged();
    emit changed();
}

void Settings::notifyLayersUpdated()
{
    save();
    emit layersChanged();
    emit changed();
}

void Settings::setOverlayPosition(int x, int y)
{
    if (m_data.overlayPosX == x && m_data.overlayPosY == y) {
        return;
    }
    m_data.overlayPosX = x;
    m_data.overlayPosY = y;
    save();
}

void Settings::setOcrRegion(int x, int y, int width, int height)
{
    m_data.ocrX = x;
    m_data.ocrY = y;
    m_data.ocrWidth = width;
    m_data.ocrHeight = height;
    save();
    emit dataSourcesChanged();
    emit changed();
}

void Settings::setInteractiveByDefault(bool enabled)
{
    if (m_data.interactiveByDefault == enabled) {
        return;
    }
    m_data.interactiveByDefault = enabled;
    save();
    emit changed();
}

void Settings::setBreadcrumbConnectLines(bool enabled)
{
    if (m_data.breadcrumbConnectLines == enabled) {
        return;
    }
    m_data.breadcrumbConnectLines = enabled;
    save();
    emit layersChanged();
    emit changed();
}

void Settings::setMiniMapShowPoiLabels(bool enabled)
{
    if (m_data.miniMapShowPoiLabels == enabled) {
        return;
    }
    m_data.miniMapShowPoiLabels = enabled;
    save();
    emit layersChanged();
    emit changed();
}

void Settings::setInteractionBinding(const QString &binding)
{
    const QString normalized = binding.trimmed().isEmpty() ? QStringLiteral("M4") : binding.trimmed();
    if (m_data.interactionBinding == normalized) {
        return;
    }
    m_data.interactionBinding = normalized;
    save();
    emit changed();
}

void Settings::setBridgeMapSize(double mapSize)
{
    mapSize = qBound(100.0, mapSize, 4000.0);
    if (qFuzzyCompare(m_data.boschMapSize, mapSize)) {
        return;
    }
    m_data.boschMapSize = mapSize;
    save();
    emit changed();
}

void Settings::setIndividualZoneEnabled(const QString &layer, const QString &name, bool enabled)
{
    QHash<QString, bool> *target = nullptr;
    if (layer == QStringLiteral("migrations")) {
        target = &m_data.individualMigrations;
    } else if (layer == QStringLiteral("patrol_zones")) {
        target = &m_data.individualPatrols;
    } else if (layer == QStringLiteral("sanctuaries")) {
        target = &m_data.individualSanctuaries;
    }
    if (target == nullptr) {
        return;
    }
    if (target->value(name, true) == enabled) {
        return;
    }
    target->insert(name, enabled);
    save();
    emit layersChanged();
    emit changed();
}

} // namespace isle
