#include "ui/DataSourcePanel.h"

#include "core/Settings.h"

#include <QCheckBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace isle {

DataSourcePanel::DataSourcePanel(Settings &settings, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *title = new QLabel(QStringLiteral("Position data sources"), this);
    title->setStyleSheet(QStringLiteral("font-weight: 600;"));
    layout->addWidget(title);

    m_hint = new QLabel(this);
    m_hint->setWordWrap(true);
    layout->addWidget(m_hint);

    m_clipboard = new QCheckBox(QStringLiteral("Clipboard (Copy Location)"), this);
    m_ocr = new QCheckBox(QStringLiteral("OCR / Automatic Tracking (Windows)"), this);
    m_bosch = new QCheckBox(QStringLiteral("Live map bridge"), this);
    layout->addWidget(m_clipboard);
    layout->addWidget(m_ocr);
    layout->addWidget(m_bosch);
    layout->addStretch(1);

    connect(m_clipboard, &QCheckBox::toggled, this, &DataSourcePanel::clipboardToggled);
    connect(m_ocr, &QCheckBox::toggled, this, &DataSourcePanel::ocrToggled);
    connect(m_bosch, &QCheckBox::toggled, this, &DataSourcePanel::boschToggled);
    connect(&m_settings, &Settings::dataSourcesChanged, this, &DataSourcePanel::syncFromSettings);
    connect(&m_settings, &Settings::liveMapProviderChanged, this, &DataSourcePanel::syncFromSettings);

    syncFromSettings();
}

void DataSourcePanel::syncFromSettings()
{
    const auto &sources = m_settings.data().sources;
    const QString server = liveMapProviderLabel(m_settings.data().liveMapProvider);
    m_hint->setText(
        QStringLiteral("Active live map server: %1. Turn off any source you do not want updating the map.")
            .arg(server));
    m_bosch->setText(QStringLiteral("Live map bridge (%1)").arg(server));
    const QSignalBlocker b1(m_clipboard);
    const QSignalBlocker b2(m_ocr);
    const QSignalBlocker b3(m_bosch);
    m_clipboard->setChecked(sources.clipboard);
    m_ocr->setChecked(sources.ocr);
    m_bosch->setChecked(sources.liveMap);
}

} // namespace isle
