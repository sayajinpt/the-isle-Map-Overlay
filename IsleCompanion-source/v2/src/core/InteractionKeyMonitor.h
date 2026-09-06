#pragma once

#include <QObject>
#include <QTimer>
#include <QVector>

namespace isle {

// Poll-only toggle for Mini Map edit/click-through (default M4 / XBUTTON1).
// Does not register or consume the key — same approach as v1.
class InteractionKeyMonitor : public QObject {
    Q_OBJECT
public:
    explicit InteractionKeyMonitor(QObject *parent = nullptr);

    bool setBinding(const QString &binding);
    void start();
    void stop();
    void setActive(bool active);
    bool isActive() const { return m_active; }

signals:
    void toggled(bool active);
    void bindingError(const QString &message);

private slots:
    void poll();

private:
    QTimer m_timer;
    QVector<int> m_keys;
    bool m_active = false;
    bool m_physicalPressed = false;

    bool readCombination() const;
};

} // namespace isle
