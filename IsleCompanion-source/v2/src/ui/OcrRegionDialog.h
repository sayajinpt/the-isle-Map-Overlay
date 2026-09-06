#pragma once

#include <QDialog>
#include <QRect>

namespace isle {

class OcrRegionDialog : public QDialog {
    Q_OBJECT
public:
    explicit OcrRegionDialog(QWidget *parent = nullptr);
    QRect selectedRegion() const { return m_region; }

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QPoint m_origin;
    QRect m_region;
    bool m_dragging = false;
};

} // namespace isle
