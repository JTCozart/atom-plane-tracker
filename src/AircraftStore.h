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

    int historyCount() const          { return _historyCount; }
    int historyIndex() const          { return _historyIndex; }
    void setHistoryIndex(int i)       { _historyIndex = i; }
    const Aircraft& historyAt(int i) const { return _history[i]; }

    int detectionCount(AircraftClass classification) const { return _detectionCounts[toIndex(classification)]; }

    const std::vector<String>& apiResponseLines() const { return _apiResponseLines; }
    int lastResponseCode()  const { return _lastResponseCode; }
    int lastAircraftCount() const { return _lastAircraftCount; }

private:
    std::map<String, Aircraft> _activeAircraft;
    String                     _displayKey;
    uint32_t                   _lastCycleTime = 0;

    Aircraft _history[5];
    int      _historyCount = 0;
    int      _historyIndex = 0;

    int _detectionCounts[4] = {0, 0, 0, 0};

    std::vector<String> _apiResponseLines;
    int _lastResponseCode  = 0;
    int _lastAircraftCount = 0;

    void recordInHistory(const Aircraft& aircraft);
};
