#include "ScreenController.h"
#include "AircraftStore.h"

void ScreenController::setModeFromString(const String& name) {
    if      (name == "scan")    _mode = ScreenMode::Scanning;
    else if (name == "history") _mode = ScreenMode::History;
    else if (name == "summary") _mode = ScreenMode::Summary;
    else if (name == "radar")   _mode = ScreenMode::Radar;
    else if (name == "debug")   _mode = ScreenMode::Debug;
    _debugScrollOffset = 0;
    _webControlChanged = true;
}

void ScreenController::toggleDebug() {
    if (_mode == ScreenMode::Debug) {
        _mode = _modeBeforeDebug;
    } else {
        _modeBeforeDebug   = _mode;
        _debugScrollOffset = 0;
        _mode              = ScreenMode::Debug;
    }
}

void ScreenController::onNewAircraftDetected() {
    if (_mode != ScreenMode::Debug && _mode != ScreenMode::Radar)
        _mode = ScreenMode::Scanning;
}

bool ScreenController::handleShortPress(AircraftStore& store) {
    switch (_mode) {
        case ScreenMode::Scanning:
            _mode = (store.historyCount() > 0) ? ScreenMode::History : ScreenMode::Summary;
            break;
        case ScreenMode::History:
            if (store.historyIndex() < store.historyCount() - 1)
                store.setHistoryIndex(store.historyIndex() + 1);
            else {
                store.setHistoryIndex(0);
                _mode = ScreenMode::Summary;
            }
            break;
        case ScreenMode::Summary: {
            uint32_t now = millis();
            if (_lastClickTime > 0 && now - _lastClickTime <= kDoubleClickWindowMs) {
                _lastClickTime = 0;
                return true;  // double-click: caller should call store.clearCounts()
            }
            // First click: record time and stay on Summary.
            // update() advances to Radar once the window expires.
            _lastClickTime = now;
            break;
        }
        case ScreenMode::Radar:
            _mode = ScreenMode::Scanning;
            break;
        default: break;
    }
    return false;
}

bool ScreenController::handleDebugShortPress(int totalLines, int pageSize) {
    uint32_t now = millis();
    if (now - _lastClickTime <= kDoubleClickWindowMs) {
        _lastClickTime = 0;
        return true;  // caller should send test notification
    }
    _lastClickTime = now;
    int maxScroll      = max(0, totalLines - pageSize);
    _debugScrollOffset = (_debugScrollOffset >= maxScroll) ? 0 : _debugScrollOffset + pageSize;
    return false;
}

bool ScreenController::consumeWebControlChange() {
    bool changed      = _webControlChanged;
    _webControlChanged = false;
    return changed;
}

bool ScreenController::update(uint32_t now) {
    if (_mode == ScreenMode::Summary && _lastClickTime > 0
            && now - _lastClickTime > kDoubleClickWindowMs) {
        _lastClickTime = 0;
        _mode = ScreenMode::Radar;
        return true;
    }
    return false;
}
