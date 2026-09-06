#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

namespace isle {

enum class LiveMapProvider {
    Bosch = 0,
    VoiceIsland = 1,
};

inline QString liveMapProviderId(LiveMapProvider provider)
{
    switch (provider) {
    case LiveMapProvider::VoiceIsland:
        return QStringLiteral("voice_island");
    case LiveMapProvider::Bosch:
    default:
        return QStringLiteral("bosch");
    }
}

inline LiveMapProvider liveMapProviderFromId(const QString &id)
{
    if (id.compare(QStringLiteral("voice_island"), Qt::CaseInsensitive) == 0
        || id.compare(QStringLiteral("voice-island"), Qt::CaseInsensitive) == 0) {
        return LiveMapProvider::VoiceIsland;
    }
    return LiveMapProvider::Bosch;
}

inline QString liveMapProviderLabel(LiveMapProvider provider)
{
    switch (provider) {
    case LiveMapProvider::VoiceIsland:
        return QStringLiteral("Voice Island");
    case LiveMapProvider::Bosch:
    default:
        return QStringLiteral("Bosch Island");
    }
}

inline QString liveMapProviderUrl(LiveMapProvider provider)
{
    switch (provider) {
    case LiveMapProvider::VoiceIsland:
        return QStringLiteral("https://voice-island.com/dashboard/livemap");
    case LiveMapProvider::Bosch:
    default:
        return QStringLiteral("https://bosch-island.com/map-tracker");
    }
}

inline QString liveMapUserscriptFileName(LiveMapProvider provider)
{
    switch (provider) {
    case LiveMapProvider::VoiceIsland:
        return QStringLiteral("voice_island_bridge.user.js");
    case LiveMapProvider::Bosch:
    default:
        return QStringLiteral("bosch_island_bridge.user.js");
    }
}

struct DataSourceToggles {
    bool clipboard = true;
    bool ocr = false;
    bool liveMap = false; // localhost bridge for Bosch / Voice Island
};

struct ServerProfileData {
    bool liveMapEnabled = false;
    double bridgeMapSize = 750.0;
    bool primeRunActive = true;
    QSet<QString> visitedZoneIds;
    QHash<QString, bool> individualMigrations;
    QHash<QString, bool> individualPatrols;
    QHash<QString, bool> individualSanctuaries;
};

struct SettingsData {
    LiveMapProvider liveMapProvider = LiveMapProvider::Bosch;
    DataSourceToggles sources;
    bool alwaysOnTop = true;
    bool playerCentered = true;
    double overlayOpacity = 0.92;
    int overlaySize = 360;
    QString overlayShape = QStringLiteral("circle");
    int boschPort = 8765; // shared localhost bridge port
    double boschMapSize = 750.0; // active profile bridge map size
    bool breadcrumbsEnabled = true;
    bool breadcrumbConnectLines = true;
    int breadcrumbMaxPoints = 500;
    bool primeRunActive = true;
    bool miniMapShowPoiLabels = false;
    bool interactiveByDefault = true;
    int overlayPosX = -1;
    int overlayPosY = -1;
    int ocrX = 0;
    int ocrY = 0;
    int ocrWidth = 0;
    int ocrHeight = 0;
    QString interactionBinding = QStringLiteral("M4");
    QSet<QString> visitedZoneIds;
    QHash<QString, bool> individualMigrations;
    QHash<QString, bool> individualPatrols;
    QHash<QString, bool> individualSanctuaries;
    QHash<QString, ServerProfileData> profiles;
    QHash<QString, bool> layers = {
        {QStringLiteral("migrations"), true},
        {QStringLiteral("patrol_zones"), true},
        {QStringLiteral("sanctuaries"), true},
        {QStringLiteral("updrafts"), true},
        {QStringLiteral("water"), true},
        {QStringLiteral("locations"), true},
        {QStringLiteral("food"), false},
        {QStringLiteral("ai"), false},
        {QStringLiteral("salt_licks"), false},
        {QStringLiteral("spawns"), false},
        {QStringLiteral("player"), true},
        {QStringLiteral("breadcrumbs"), true},
    };
};

class Settings : public QObject {
    Q_OBJECT
public:
    explicit Settings(QObject *parent = nullptr);

    const SettingsData &data() const { return m_data; }
    SettingsData &data() { return m_data; }

    void load();
    void save() const;

    void setClipboardEnabled(bool enabled);
    void setOcrEnabled(bool enabled);
    void setLiveMapEnabled(bool enabled);
    void setBoschEnabled(bool enabled); // alias → live map
    void setLiveMapProvider(LiveMapProvider provider);
    void setPlayerCentered(bool enabled);
    void setAlwaysOnTop(bool enabled);
    void setOverlaySize(int size);
    void setOverlayOpacity(double opacity);
    void setOverlayShape(const QString &shape);
    void setLayerEnabled(const QString &layer, bool enabled);
    void setOverlayPosition(int x, int y);
    void setOcrRegion(int x, int y, int width, int height);
    void setInteractiveByDefault(bool enabled);
    void setBreadcrumbConnectLines(bool enabled);
    void setMiniMapShowPoiLabels(bool enabled);
    void setInteractionBinding(const QString &binding);
    void setIndividualZoneEnabled(const QString &layer, const QString &name, bool enabled);
    void setBridgeMapSize(double mapSize);
    void notifyLayersUpdated();

signals:
    void changed();
    void dataSourcesChanged();
    void layersChanged();
    void liveMapProviderChanged(LiveMapProvider provider);

private:
    SettingsData m_data;
    QString m_path;

    static SettingsData defaults();
    static SettingsData fromJson(const QJsonObject &object);
    static QJsonObject toJson(const SettingsData &data);
    static ServerProfileData profileFromActive(const SettingsData &data);
    static void applyProfile(SettingsData *data, const ServerProfileData &profile);
    static void stashActiveProfile(SettingsData *data);
    static ServerProfileData profileFromJson(const QJsonObject &object);
    static QJsonObject profileToJson(const ServerProfileData &profile);
};

} // namespace isle
