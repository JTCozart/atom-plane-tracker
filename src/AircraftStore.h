#pragma once
#include <Arduino.h>
#include <map>
#include <vector>
#include "Aircraft.h"
#include "AppState.h"
#include "Config.h"
#include "Notifier.h"

class AircraftStore {
public:
    static constexpr uint32_t kCycleIntervalMs = 5000;

    // Fetch latest aircraft data from the API and update internal state.
    // Sets mode to ScreenMode::Scanning when a new aircraft arrives (unless ScreenMode::Debug).
    void fetch(const Config& cfg, Notifier& notifier, ScreenMode& mode);

    // Returns true if there are any aircraft currently in radius.
    bool hasActiveAircraft() const;

    // Returns the number of aircraft currently in radius.
    int activeAircraftCount() const;

    // Returns a pointer to the currently-cycling live aircraft, or nullptr if none.
    const Aircraft* currentAircraft() const;

    // Advance _displayKey to the next aircraft in the map.
    void cycleToNextAircraft();

    uint32_t lastCycleTime() const { return _lastCycleTime; }

    // Device-navigable history (capped at kDeviceHistoryMax for button UX).
    int historyCount() const          { return min(_historyCount, kDeviceHistoryMax); }
    int historyIndex() const          { return _historyIndex; }
    void setHistoryIndex(int i)       { _historyIndex = i; }
    const Aircraft& historyAt(int i) const { return _history[i]; }
    // Full history available to the web UI.
    int webHistoryCount() const       { return _historyCount; }

    int detectionCount(AircraftClass classification) const { return _detectionCounts[toIndex(classification)]; }

    // Persist detection counts to NVS so they survive reboots.
    void loadCountsFromNVS();
    void saveCountsToNVS() const;
    void clearCounts();

    const std::vector<String>& apiResponseLines() const { return _apiResponseLines; }
    int lastResponseCode()    const { return _lastResponseCode; }
    int lastAircraftCount()   const { return _lastAircraftCount; }
    int consecutiveFailures() const { return _consecutiveFailures; }

    const std::map<String, Aircraft>& activeAircraft() const { return _activeAircraft; }

private:
    std::map<String, Aircraft> _activeAircraft;
    String                     _displayKey;
    uint32_t                   _lastCycleTime = 0;

    static constexpr int kDeviceHistoryMax = 5;
    static constexpr int kWebHistoryMax    = 20;

    Aircraft _history[kWebHistoryMax];
    int      _historyCount = 0;
    int      _historyIndex = 0;

    int _detectionCounts[4] = {0, 0, 0, 0};

    std::vector<String> _apiResponseLines;
    int _lastResponseCode    = 0;
    int _lastAircraftCount   = 0;
    int _consecutiveFailures = 0;

    void recordInHistory(const Aircraft& aircraft);
};
