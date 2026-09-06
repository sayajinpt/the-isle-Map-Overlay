#pragma once

#include "core/Settings.h"

#include <QDialog>

namespace isle {

class ServerChooserDialog : public QDialog {
    Q_OBJECT
public:
    explicit ServerChooserDialog(LiveMapProvider current, QWidget *parent = nullptr);

    LiveMapProvider selectedProvider() const { return m_selected; }

private:
    LiveMapProvider m_selected = LiveMapProvider::Bosch;
};

} // namespace isle
