#pragma once

#include <QWidget>

class QCheckBox;
class QLabel;

namespace isle {

class Settings;

class DataSourcePanel : public QWidget {
    Q_OBJECT
public:
    explicit DataSourcePanel(Settings &settings, QWidget *parent = nullptr);

public slots:
    void syncFromSettings();

signals:
    void clipboardToggled(bool enabled);
    void ocrToggled(bool enabled);
    void boschToggled(bool enabled);

private:
    Settings &m_settings;
    QCheckBox *m_clipboard = nullptr;
    QCheckBox *m_ocr = nullptr;
    QCheckBox *m_bosch = nullptr;
    QLabel *m_hint = nullptr;
};

} // namespace isle
