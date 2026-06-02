#include "AircraftStore.h"
#include <ArduinoJson.h>
#include <map>
#include <set>

// Returns true if `type` (case-insensitive) is found as a whole token in the comma-separated `poiList`.
// Zero heap allocation: walks poiList token-by-token with strncasecmp.
static bool typeMatchesPoi(const char* type, const char* poiList) {
    if (!type || type[0] == '\0' || !poiList || poiList[0] == '\0') return false;
    size_t typeLen = strlen(type);
    const char* p = poiList;
    while (*p) {
        const char* end = strchr(p, ',');
        if (!end) end = p + strlen(p);
        if ((size_t)(end - p) == typeLen && strncasecmp(p, type, typeLen) == 0) return true;
        p = (*end == ',') ? end + 1 : end;
    }
    return false;
}

// ── History ───────────────────────────────────────────────────────────────────

void AircraftStore::recordInHistory(const Aircraft& aircraft) {
    int slots = min(_historyCount, AircraftPersistence::kWebHistoryMax - 1);
    for (int i = slots; i > 0; i--) _history[i] = _history[i - 1];
    _history[0] = aircraft;
    if (_historyCount < AircraftPersistence::kWebHistoryMax) _historyCount++;
    _historyIndex = 0;
    _persistence.saveHistory(_history, _historyCount);
}

// ── Detection count persistence ───────────────────────────────────────────────

void AircraftStore::loadCountsFromNVS() {
    _persistence.loadCounts(_detectionCounts);
}

void AircraftStore::clearCounts() {
    for (int& count : _detectionCounts) count = 0;
    _persistence.saveCounts(_detectionCounts);
}

// ── History persistence ───────────────────────────────────────────────────────

void AircraftStore::loadHistoryFromNVS() {
    _historyCount = _persistence.loadHistory(_history);
    _historyIndex = 0;
}

// ── Active aircraft accessors ─────────────────────────────────────────────────

bool AircraftStore::hasActiveAircraft() const {
    return !_activeAircraft.empty();
}

int AircraftStore::activeAircraftCount() const {
    return (int)_activeAircraft.size();
}

const Aircraft* AircraftStore::currentAircraft() const {
    if (_activeAircraft.empty()) return nullptr;
    auto it = _activeAircraft.find(_displayKey);
    if (it == _activeAircraft.end()) it = _activeAircraft.begin();
    return &it->second;
}

void AircraftStore::cycleToNextAircraft() {
    if (_activeAircraft.empty()) return;
    auto it = _activeAircraft.find(_displayKey);
    if (it == _activeAircraft.end() || ++it == _activeAircraft.end())
        it = _activeAircraft.begin();
    _displayKey    = it->first;
    _lastCycleTime = millis();
}

// ── Fetch helpers ─────────────────────────────────────────────────────────────

void AircraftStore::buildDebugLines(JsonArray aircraftArray) {
    _apiResponseLines.clear();
    for (JsonObject entry : aircraftArray) {
        String icao         = entry["hex"]    | "??????"; icao.toUpperCase();
        String callsign     = entry["flight"] | ""; callsign.trim();
        String registration = entry["r"]      | "";
        String type         = entry["t"]      | "??";
        String squawk       = entry["squawk"] | "";
        float  altitude     = entry["alt_baro"] | 0.0f;
        bool   isMilitary   = (entry["mil"] | 0) != 0 || ((entry["dbFlags"] | 0) & 1) != 0;

        char line[48];
        snprintf(line, sizeof(line), "%s %s %s",
                 icao.c_str(),
                 callsign.length() ? callsign.c_str() : registration.c_str(),
                 type.c_str());
        _apiResponseLines.push_back(line);

        snprintf(line, sizeof(line), " sqk:%s mil:%c %5.0fft",
                 squawk.length() ? squawk.c_str() : "----",
                 isMilitary ? 'Y' : 'N', altitude);
        _apiResponseLines.push_back(line);
        _apiResponseLines.push_back("---");
    }
    if (_apiResponseLines.empty()) _apiResponseLines.push_back("No aircraft");
}

void AircraftStore::updateExistingAircraft(Aircraft& existing, JsonObject entry,
                                           const Config& cfg, Notifier& notifier) {
    existing.altitude          = entry["alt_baro"] | 0.0f;
    existing.latitude          = entry["lat"]      | 0.0f;
    existing.longitude         = entry["lon"]      | 0.0f;
    existing.groundSpeed       = entry["gs"]        | 0.0f;
    existing.trackDegrees      = entry["track"]    | 0.0f;
    existing.positionTimestamp = millis();

    String squawk = entry["squawk"] | "";
    if (squawk != existing.squawk && Aircraft::isEmergencySquawkCode(squawk)) {
        existing.squawk = squawk;
        notifier.notifyEmergencySquawk(existing, cfg);
    } else {
        existing.squawk = squawk;
    }
}

void AircraftStore::registerNewAircraft(const String& icao, JsonObject entry,
                                        const Config& cfg, Notifier& notifier) {
    bool   isMilitary   = (entry["mil"] | 0) != 0 || ((entry["dbFlags"] | 0) & 1) != 0;
    String type         = entry["t"]        | "";
    String callsign     = entry["flight"]   | ""; callsign.trim();
    String registration = entry["r"]        | "";
    String owner        = entry["ownOp"]    | "";
    String category     = entry["category"] | "";
    String squawk       = entry["squawk"]   | "";
    float  altitude     = entry["alt_baro"] | 0.0f;
    float  latitude     = entry["lat"]      | 0.0f;
    float  longitude    = entry["lon"]      | 0.0f;
    float  groundSpeed  = entry["gs"]       | 0.0f;
    float  trackDegrees = entry["track"]    | 0.0f;

    AircraftClass classification = Aircraft::classify(callsign, owner, isMilitary, category);
    Aircraft aircraft = { icao, callsign, registration, type, owner, squawk,
                          altitude, latitude, longitude, groundSpeed, trackDegrees,
                          millis(), classification };

    _activeAircraft[icao] = aircraft;
    _detectionCounts[toIndex(classification)]++;
    _persistence.saveCounts(_detectionCounts);
    recordInHistory(aircraft);

    Serial.printf("[DETECT] %s %s %s  alt=%.0f  class=%d\n",
                  icao.c_str(),
                  callsign.length() ? callsign.c_str() : registration.c_str(),
                  type.c_str(), altitude, (int)classification);

    _displayKey    = icao;
    _lastCycleTime = millis();

    notifier.notifyDetection(aircraft, cfg);
    if (aircraft.isEmergencySquawk()) notifier.notifyEmergencySquawk(aircraft, cfg);
}

void AircraftStore::pruneStaleAircraft(const std::set<String>& seen) {
    for (auto it = _activeAircraft.begin(); it != _activeAircraft.end(); ) {
        if (!seen.count(it->first)) it = _activeAircraft.erase(it);
        else                        ++it;
    }
    if (!_activeAircraft.empty() && !_activeAircraft.count(_displayKey)) {
        _displayKey    = _activeAircraft.begin()->first;
        _lastCycleTime = millis();
    }
}

// ── Fetch ─────────────────────────────────────────────────────────────────────

FetchEffect AircraftStore::fetch(const Config& cfg, Notifier& notifier) {
    JsonDocument doc;
    if (!_source.fetch(cfg, doc)) return FetchEffect::None;

    JsonArray aircraftArray = doc["ac"].as<JsonArray>();
    _lastAircraftCount = aircraftArray.size();
    Serial.printf("[SCAN] %d aircraft in range\n", _lastAircraftCount);

    buildDebugLines(aircraftArray);

    bool newAircraftDetected = false;
    std::set<String> seen;
    for (JsonObject entry : aircraftArray) {
        const char* hex = entry["hex"] | "";
        if (!hex || hex[0] == '\0') continue;

        String icao = hex;
        icao.toUpperCase();

        // POI display filter applies to both new arrivals and ongoing updates.
        // Aircraft not in `seen` are evicted by pruneStaleAircraft, so pre-existing
        // non-POI entries are naturally removed on the next scan.
        if (cfg.poiDisplayActive()) {
            const char* type = entry["t"] | "";
            if (!typeMatchesPoi(type, cfg.poiTypes)) continue;
        }

        seen.insert(icao);

        if (_activeAircraft.count(icao)) {
            updateExistingAircraft(_activeAircraft[icao], entry, cfg, notifier);
        } else {
            registerNewAircraft(icao, entry, cfg, notifier);
            newAircraftDetected = true;
        }
    }

    pruneStaleAircraft(seen);
    return newAircraftDetected ? FetchEffect::NewAircraftDetected : FetchEffect::None;
}
