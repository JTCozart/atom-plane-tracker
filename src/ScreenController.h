#pragma once
#include <Arduino.h>
#include "AppState.h"

class AircraftStore;  // forward declaration — avoids circular header dependency

class ScreenController {
public:
    ScreenMode    current()          const { return _mode; }
    bool          isDebugMode()      const { return _mode == ScreenMode::Debug; }
    int           debugScrollOffset() const { return _debugScrollOffset; }
    const String& pendingPin()              const { return _pendingPin; }
    const char*   pendingPinTitle()         const { return _pendingPinTitle; }
    void          setPendingPin(const String& pin, const char* title = "RESET PIN") {
        _pendingPin      = pin;
        _pendingPinTitle = title;
    }

    void setMode(ScreenMode mode) { _mode = mode; }
    void setModeFromString(const String& name);

    // Signal a web-triggered change without changing mode (e.g. history paging).
    void markChanged() { _webControlChanged = true; }

    // Toggle debug overlay on/off, saving and restoring the previous mode.
    void toggleDebug();

    // Called when a new aircraft is detected; switches to Scanning unless in Debug/Radar.
    void onNewAircraftDetected();

    // Advances through screen modes on button short-press.
    // Returns true if the summary count should be cleared (double-click on Summary).
    bool handleShortPress(AircraftStore& store);

    // Handles a short press while in Debug mode.
    // Returns true if a test notification should be sent (double-click).
    // Otherwise advances the debug scroll position.
    bool handleDebugShortPress(int totalLines, int pageSize);

    // Returns true (and clears the flag) if a web control request changed state.
    bool consumeWebControlChange();

    // Returns true (and clears the flag) if a button press cancelled an active PinDisplay.
    bool consumePinCancelled();

    // Call every loop iteration. Advances Summary → Radar after the double-click
    // window expires with no second press. Returns true if the mode changed (re-render needed).
    bool update(uint32_t now);

private:
    static constexpr uint32_t kDoubleClickWindowMs = 400;

    ScreenMode _mode              = ScreenMode::Scanning;
    ScreenMode _modeBeforeDebug   = ScreenMode::Scanning;
    bool       _webControlChanged = false;
    uint32_t   _lastClickTime     = 0;
    int        _debugScrollOffset  = 0;
    String      _pendingPin;
    const char* _pendingPinTitle{"RESET PIN"};
    bool        _pinCancelled{false};
};
