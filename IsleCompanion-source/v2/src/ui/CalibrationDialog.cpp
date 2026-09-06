#include "ui/CalibrationDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace isle {

namespace {

QDoubleSpinBox *makeSpin(QWidget *parent, double value)
{
    auto *spin = new QDoubleSpinBox(parent);
    spin->setDecimals(3);
    spin->setRange(-10'000'000.0, 10'000'000.0);
    spin->setValue(value);
    return spin;
}

} // namespace

CalibrationDialog::CalibrationDialog(MapCalibration calibration,
                                     const QString &path,
                                     QWidget *parent)
    : QDialog(parent)
    , m_calibration(std::move(calibration))
    , m_path(path)
{
    setWindowTitle(QStringLiteral("Map calibration"));
    resize(420, 480);
    auto *root = new QVBoxLayout(this);
    root->addWidget(new QLabel(
        QStringLiteral("Advanced: align Gateway world coordinates with the map image."),
        this));

    auto *form = new QFormLayout();
    m_worldMinX = makeSpin(this, m_calibration.worldMinX);
    m_worldMaxX = makeSpin(this, m_calibration.worldMaxX);
    m_worldMinY = makeSpin(this, m_calibration.worldMinY);
    m_worldMaxY = makeSpin(this, m_calibration.worldMaxY);
    m_pixelMinX = makeSpin(this, m_calibration.pixelMinX);
    m_pixelMaxX = makeSpin(this, m_calibration.pixelMaxX);
    m_pixelMinY = makeSpin(this, m_calibration.pixelMinY);
    m_pixelMaxY = makeSpin(this, m_calibration.pixelMaxY);
    form->addRow(QStringLiteral("World min X"), m_worldMinX);
    form->addRow(QStringLiteral("World max X"), m_worldMaxX);
    form->addRow(QStringLiteral("World min Y"), m_worldMinY);
    form->addRow(QStringLiteral("World max Y"), m_worldMaxY);
    form->addRow(QStringLiteral("Pixel min X"), m_pixelMinX);
    form->addRow(QStringLiteral("Pixel max X"), m_pixelMaxX);
    form->addRow(QStringLiteral("Pixel min Y"), m_pixelMinY);
    form->addRow(QStringLiteral("Pixel max Y"), m_pixelMaxY);
    root->addLayout(form);

    m_invertX = new QCheckBox(QStringLiteral("Invert X"), this);
    m_invertY = new QCheckBox(QStringLiteral("Invert Y"), this);
    m_swapAxes = new QCheckBox(QStringLiteral("Swap axes"), this);
    m_invertX->setChecked(m_calibration.invertX);
    m_invertY->setChecked(m_calibration.invertY);
    m_swapAxes->setChecked(m_calibration.swapAxes);
    root->addWidget(m_invertX);
    root->addWidget(m_invertY);
    root->addWidget(m_swapAxes);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &CalibrationDialog::saveAndAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &CalibrationDialog::reject);
    root->addWidget(buttons);
}

void CalibrationDialog::saveAndAccept()
{
    m_calibration.worldMinX = m_worldMinX->value();
    m_calibration.worldMaxX = m_worldMaxX->value();
    m_calibration.worldMinY = m_worldMinY->value();
    m_calibration.worldMaxY = m_worldMaxY->value();
    m_calibration.pixelMinX = m_pixelMinX->value();
    m_calibration.pixelMaxX = m_pixelMaxX->value();
    m_calibration.pixelMinY = m_pixelMinY->value();
    m_calibration.pixelMaxY = m_pixelMaxY->value();
    m_calibration.invertX = m_invertX->isChecked();
    m_calibration.invertY = m_invertY->isChecked();
    m_calibration.swapAxes = m_swapAxes->isChecked();
    if (!saveCalibration(m_path, m_calibration)) {
        return;
    }
    accept();
}

} // namespace isle
