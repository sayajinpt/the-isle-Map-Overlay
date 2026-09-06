#include "core/InteractionKeyMonitor.h"

#ifdef ISLE_COMPANION_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <QHash>

namespace isle {
namespace {

int virtualKeyForName(const QString &name)
{
    static const QHash<QString, int> keys = {
        {QStringLiteral("CTRL"), 0x11},
        {QStringLiteral("CONTROL"), 0x11},
        {QStringLiteral("SHIFT"), 0x10},
        {QStringLiteral("ALT"), 0x12},
        {QStringLiteral("WIN"), 0x5B},
        {QStringLiteral("WINDOWS"), 0x5B},
        {QStringLiteral("M4"), 0x05},
        {QStringLiteral("XBUTTON1"), 0x05},
        {QStringLiteral("M5"), 0x06},
        {QStringLiteral("XBUTTON2"), 0x06},
        {QStringLiteral("SPACE"), 0x20},
    };
    if (keys.contains(name)) {
        return keys.value(name);
    }
    if (name.size() == 1) {
        return name.at(0).toUpper().unicode();
    }
    if (name.startsWith(QLatin1Char('F')) && name.mid(1).toInt() >= 1) {
        return 0x70 + name.mid(1).toInt() - 1;
    }
    return 0;
}

} // namespace

InteractionKeyMonitor::InteractionKeyMonitor(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(25);
    connect(&m_timer, &QTimer::timeout, this, &InteractionKeyMonitor::poll);
    setBinding(QStringLiteral("M4"));
}

bool InteractionKeyMonitor::setBinding(const QString &binding)
{
    m_keys.clear();
    const auto parts = binding.toUpper().split(QLatin1Char('+'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        emit bindingError(QStringLiteral("interaction binding is empty"));
        return false;
    }
    for (QString part : parts) {
        part = part.trimmed();
        const int key = virtualKeyForName(part);
        if (key == 0) {
            emit bindingError(QStringLiteral("unsupported binding key: %1").arg(part));
            m_keys.clear();
            return false;
        }
        m_keys.push_back(key);
    }
    m_physicalPressed = readCombination();
    return true;
}

void InteractionKeyMonitor::start()
{
    m_physicalPressed = readCombination();
    m_timer.start();
}

void InteractionKeyMonitor::stop()
{
    m_timer.stop();
}

void InteractionKeyMonitor::setActive(bool active)
{
    if (m_active == active) {
        return;
    }
    m_active = active;
    emit toggled(m_active);
}

bool InteractionKeyMonitor::readCombination() const
{
#ifdef ISLE_COMPANION_WINDOWS
    if (m_keys.isEmpty()) {
        return false;
    }
    for (int key : m_keys) {
        if ((GetAsyncKeyState(key) & 0x8000) == 0) {
            return false;
        }
    }
    return true;
#else
    return false;
#endif
}

void InteractionKeyMonitor::poll()
{
    const bool pressed = readCombination();
    if (pressed && !m_physicalPressed) {
        setActive(!m_active);
    }
    m_physicalPressed = pressed;
}

} // namespace isle
