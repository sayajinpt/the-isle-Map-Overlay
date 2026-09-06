#pragma once

#include <QWidget>
#include <QtGui/qwindowdefs.h>

namespace isle {

#ifdef ISLE_COMPANION_WINDOWS
bool applyWindowsOverlayInputStyle(WId windowId, bool interactable);
bool excludeWindowFromCapture(WId windowId);
#else
inline bool applyWindowsOverlayInputStyle(WId, bool) { return false; }
inline bool excludeWindowFromCapture(WId) { return false; }
#endif

} // namespace isle
