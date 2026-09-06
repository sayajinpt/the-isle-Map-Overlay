#pragma once

#include "core/CoordinateTransform.h"

#include <QDialog>

class QCheckBox;
class QDoubleSpinBox;

namespace isle {

class CalibrationDialog : public QDialog {
    Q_OBJECT
public:
    CalibrationDialog(MapCalibration calibration, const QString &path, QWidget *parent = nullptr);

    MapCalibration calibration() const { return m_calibration; }

private slots:
    void saveAndAccept();

private:
    MapCalibration m_calibration;
    QString m_path;
    QDoubleSpinBox *m_worldMinX = nullptr;
    QDoubleSpinBox *m_worldMaxX = nullptr;
    QDoubleSpinBox *m_worldMinY = nullptr;
    QDoubleSpinBox *m_worldMaxY = nullptr;
    QDoubleSpinBox *m_pixelMinX = nullptr;
    QDoubleSpinBox *m_pixelMaxX = nullptr;
    QDoubleSpinBox *m_pixelMinY = nullptr;
    QDoubleSpinBox *m_pixelMaxY = nullptr;
    QCheckBox *m_invertX = nullptr;
    QCheckBox *m_invertY = nullptr;
    QCheckBox *m_swapAxes = nullptr;
};

} // namespace isle
