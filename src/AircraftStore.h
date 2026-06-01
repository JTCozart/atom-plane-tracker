#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <map>
#include <set>
#include <vector>
#include "Aircraft.h"
#include "AircraftPersistence.h"
#include "Config.h"
#include "FetchEffect.h"
#include "IAircraftSource.h"
#include "Notifier.h"

class AircraftStore {
public:
    static constexpr uint32_t kCycleIntervalMs = 5000;

    AircraftStore(IAircraftSource& source, AircraftPersistence& persistence)
        : _source(source), _persistence(persistence) {}

    // Fetches latest aircraft data and updates internal state.
    // Returns NewAircraftDetected if at least one new aircraft entered the radius.
    FetchEffect fetch(const Config& cfg, Notifier& notifier);

    bool hasActiveAircraft() const;
    int  activeAircraftCount() const;

    // Returns the currently-displayed live aircraft, or nullptr if none.
    const Aircraft* currentAircraft() const;
    void cycleToNextAircraft();

    uint32_t lastCycleTime() const { return _lastCycleTime; }

    // Device-navigable history (capped at kDeviceHistoryMax for button UX).
    int historyCount() const               { return min(_historyCount, kDeviceHistoryMax); }
    int historyIndex() const               { return _historyIndex; }
    void setHistoryIndex(int i)            { _historyIndex = i; }
    const Aircraft& historyAt(int i) const { return _history[i]; }

    // Full history available to the web UI.
    int webHistoryCount() const { return _historyCount; }

    int  detectionCount(AircraftClass classification) const { return _detectionCounts[toIndex(classification)]; }
    void clearCounts();

    void loadCountsFromNVS();
    void loadHistoryFromNVS();

    const std::vector<String>& apiResponseLines() const { return _apiResponseLines; }
    int lastResponseCode()    const { return _source.lastResponseCode(); }
    int lastAircraftCount()   const { return _lastAircraftCount; }
    int consecutiveFailures() const { return _source.consecutiveFailures(); }

    const std::map<String, Aircraft>& activeAircraft() const { return _activeAircraft; }

private:
    static constexpr int kDeviceHistoryMax = 5;

    IAircraftSource&    _source;
    AircraftPersistence& _persistence;

    std::map<String, Aircraft> _activeAircraft;
    String                     _displayKey;
    uint32_t                   _lastCycleTime = 0;

    Aircraft _history[AircraftPersistence::kWebHistoryMax];
    int      _historyCount = 0;
    int      _historyIndex = 0;

    int _detectionCounts[AircraftPersistence::kClassCount] = {};

    std::vector<String> _apiResponseLines;
    int _lastAircraftCount = 0;

    void recordInHistory(const Aircraft& aircraft);

    void buildDebugLines(JsonArray aircraftArray);
    void updateExistingAircraft(Aircraft& existing, JsonObject entry, const Config& cfg, Notifier& notifier);
    void processNewAircraft(const String& icao, JsonObject entry, const Config& cfg, Notifier& notifier);
    void pruneStaleAircraft(const std::set<String>& seen);
};
