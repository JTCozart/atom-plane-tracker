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
    static constexpr uint32_t CYCLE_MS = 5000;

    // Fetch latest aircraft data from the API and update internal state.
    // Sets mode to SCR_SCAN when a new aircraft arrives (unless SCR_DEBUG).
    void fetch(const Config& cfg, Notifier& notifier, ScreenMode& mode);

    // Returns true if there are any aircraft currently in radius.
    bool hasActive() const;

    // Returns the number of aircraft currently in radius.
    int activeCount() const;

    // Returns a pointer to the currently-cycling live aircraft, or nullptr if none.
    const Ac* displayAircraft() const;

    // Advance _liveKey to the next aircraft in the map.
    void advanceCycle();

    uint32_t cycleTimer() const { return _cycleTimer; }

    int historyCount() const     { return _histCount; }
    int historyIndex() const     { return _histIdx; }
    void setHistoryIndex(int i)  { _histIdx = i; }
    const Ac& historyAt(int i) const { return _histLog[i]; }

    int count(AcClass cls) const { return _cnt[cls]; }

    const std::vector<String>& debugLines() const { return _debugLines; }
    int lastHttpCode() const  { return _lastHttpCode; }
    int lastAcTotal()  const  { return _lastAcTotal; }

private:
    std::map<String, Ac> _inRadius;
    String               _liveKey;
    uint32_t             _cycleTimer  = 0;

    Ac  _histLog[5];
    int _histCount = 0;
    int _histIdx   = 0;

    int _cnt[4] = {0, 0, 0, 0};

    std::vector<String> _debugLines;
    int _lastHttpCode = 0;
    int _lastAcTotal  = 0;

    void addToHistory(const Ac& ac);
};
