#include "ui/ServerChooserDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace isle {

ServerChooserDialog::ServerChooserDialog(LiveMapProvider current, QWidget *parent)
    : QDialog(parent)
    , m_selected(current)
{
    setWindowTitle(QStringLiteral("Choose live map server"));
    setModal(true);
    resize(460, 280);

    auto *root = new QVBoxLayout(this);
    auto *title = new QLabel(
        QStringLiteral("Which server live map should feed the companion?"),
        this);
    title->setWordWrap(true);
    title->setStyleSheet(QStringLiteral("font-size: 14pt; font-weight: 600;"));
    root->addWidget(title);

    auto *hint = new QLabel(
        QStringLiteral(
            "Prime-run progress and per-server filters are saved separately for "
            "Bosch and Voice Island. This choice is asked every launch."),
        this);
    hint->setWordWrap(true);
    root->addWidget(hint);
    root->addSpacing(12);

    auto *bosch = new QPushButton(QStringLiteral("Bosch Island\nbosch-island.com map tracker"), this);
    bosch->setMinimumHeight(64);
    auto *voice = new QPushButton(
        QStringLiteral("Voice Island\nvoice-island.com/dashboard/livemap"), this);
    voice->setMinimumHeight(64);
    root->addWidget(bosch);
    root->addWidget(voice);
    root->addStretch(1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);

    connect(bosch, &QPushButton::clicked, this, [this]() {
        m_selected = LiveMapProvider::Bosch;
        accept();
    });
    connect(voice, &QPushButton::clicked, this, [this]() {
        m_selected = LiveMapProvider::VoiceIsland;
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (current == LiveMapProvider::VoiceIsland) {
        voice->setDefault(true);
        voice->setFocus();
    } else {
        bosch->setDefault(true);
        bosch->setFocus();
    }
}

} // namespace isle
