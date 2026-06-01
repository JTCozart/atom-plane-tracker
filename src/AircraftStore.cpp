#include "AircraftStore.h"
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <map>
#include <set>

// adsb.lol - free, no API key, ADSBExchange v2 format, radius in NM
static const char* API_FMT = "https://api.adsb.lol/v2/lat/%.6f/lon/%.6f/dist/%.1f";

// ── History ───────────────────────────────────────────────────────────────────

void AircraftStore::recordInHistory(const Aircraft& aircraft) {
    int slots = min(_historyCount, kWebHistoryMax - 1);
    for (int i = slots; i > 0; i--) _history[i] = _history[i - 1];
    _history[0] = aircraft;
    if (_historyCount < kWebHistoryMax) _historyCount++;
    _historyIndex = 0;  // always show newest on next HIST visit
}

// ── Detection count persistence ───────────────────────────────────────────────

void AircraftStore::loadCountsFromNVS() {
    Preferences prefs;
    prefs.begin("plantracker", true);
    _detectionCounts[0] = prefs.getInt("sumMil",  0);
    _detectionCounts[1] = prefs.getInt("sumMed",  0);
    _detectionCounts[2] = prefs.getInt("sumComm", 0);
    _detectionCounts[3] = prefs.getInt("sumPriv", 0);
    prefs.end();
}

void AircraftStore::saveCountsToNVS() const {
    Preferences prefs;
    prefs.begin("plantracker", false);
    prefs.putInt("sumMil",  _detectionCounts[0]);
    prefs.putInt("sumMed",  _detectionCounts[1]);
    prefs.putInt("sumComm", _detectionCounts[2]);
    prefs.putInt("sumPriv", _detectionCounts[3]);
    prefs.end();
}

void AircraftStore::clearCounts() {
    for (int i = 0; i < 4; i++) _detectionCounts[i] = 0;
    saveCountsToNVS();
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

// ── Fetch & update ────────────────────────────────────────────────────────────

void AircraftStore::fetch(const Config& cfg, Notifier& notifier, ScreenMode& mode) {
    char url[128];
    snprintf(url, sizeof(url), API_FMT,
             cfg.latitude, cfg.longitude, (double)cfg.radius);

    WiFiClientSecure client;
    client.setInsecure();  // skip cert validation on embedded
    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(8000);

    Serial.printf("[SCAN] GET %s\n", url);
    int httpCode = http.GET();
    _lastResponseCode = httpCode;
    Serial.printf("[SCAN] HTTP %d\n", httpCode);
    if (httpCode != 200) {
        http.end();
        _consecutiveFailures++;
        Serial.printf("[SCAN] fail - consecutive failures: %d\n", _consecutiveFailures);
        return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, http.getStream())) {
        http.end();
        _consecutiveFailures++;
        Serial.println("[SCAN] JSON parse error");
        return;
    }
    http.end();

    _consecutiveFailures = 0;

    // Rebuild debug lines from raw response
    JsonArray acArr = doc["ac"].as<JsonArray>();
    _lastAircraftCount = acArr.size();
    Serial.printf("[SCAN] %d aircraft in range\n", _lastAircraftCount);
    _apiResponseLines.clear();
    for (JsonObject plane : acArr) {
        String icao = plane["hex"]      | "??????";  icao.toUpperCase();
        String cs   = plane["flight"]   | "";        cs.trim();
        String reg  = plane["r"]        | "";
        String type = plane["t"]        | "??";
        String cat  = plane["category"] | "?";
        String own  = plane["ownOp"]    | "";
        String sqwk = plane["squawk"]   | "";
        float  alt  = plane["alt_baro"] | 0.0f;
        bool   mil  = (plane["mil"] | 0) != 0 || ((plane["dbFlags"] | 0) & 1) != 0;

        char buf[48];
        snprintf(buf, sizeof(buf), "%s %s %s", icao.c_str(),
                 cs.length() ? cs.c_str() : reg.c_str(), type.c_str());
        _apiResponseLines.push_back(buf);

        snprintf(buf, sizeof(buf), " sqk:%s mil:%c %5.0fft",
                 sqwk.length() ? sqwk.c_str() : "----", mil ? 'Y' : 'N', alt);
        _apiResponseLines.push_back(buf);
        _apiResponseLines.push_back("---");
    }
    if (_apiResponseLines.empty()) _apiResponseLines.push_back("No aircraft");

    // Don't reset scroll if user is actively viewing debug
    // (scroll is owned by the caller / main.cpp - nothing to reset here)

    std::set<String> seen;

    for (JsonObject plane : acArr) {
        const char* hex = plane["hex"] | "";
        if (!hex || hex[0] == '\0') continue;

        String icao = hex;
        icao.toUpperCase();
        seen.insert(icao);

        float  alt   = plane["alt_baro"] | 0.0f;
        float  lat   = plane["lat"]      | 0.0f;
        float  lon   = plane["lon"]      | 0.0f;
        float  gs    = plane["gs"]       | 0.0f;
        float  track = plane["track"]    | 0.0f;

        String squawk = plane["squawk"] | "";

        if (_activeAircraft.count(icao)) {
            // Refresh position/speed data so ETA stays accurate
            Aircraft& existing        = _activeAircraft[icao];
            existing.altitude         = alt;
            existing.latitude         = lat;
            existing.longitude        = lon;
            existing.groundSpeed      = gs;
            existing.trackDegrees     = track;
            existing.positionTimestamp = millis();
            // Notify if squawk changes to an emergency code mid-flight
            if (squawk != existing.squawk && Aircraft::isEmergencySquawkCode(squawk)) {
                existing.squawk = squawk;
                notifier.notifyEmergencySquawk(existing, cfg);
            } else {
                existing.squawk = squawk;
            }
            continue;
        }

        bool milFlag = (plane["mil"] | 0) != 0 || ((plane["dbFlags"] | 0) & 1) != 0;

        const char* cs_raw = plane["flight"] | "";
        String callsign = cs_raw;
        callsign.trim();

        String registration = plane["r"]        | "";
        String type         = plane["t"]        | "";
        String owner        = plane["ownOp"]    | "";
        String cat          = plane["category"] | "";

        AircraftClass classification = Aircraft::classify(callsign, owner, milFlag, cat);

        Aircraft aircraft = { icao, callsign, registration, type, owner, squawk, alt, lat, lon, gs, track, millis(), classification };
        _activeAircraft[icao] = aircraft;
        _detectionCounts[toIndex(classification)]++;
        saveCountsToNVS();
        recordInHistory(aircraft);

        Serial.printf("[DETECT] %s %s %s  alt=%.0f  class=%d\n",
                      icao.c_str(),
                      callsign.length() ? callsign.c_str() : registration.c_str(),
                      type.c_str(), alt, (int)classification);

        // Interrupt to live view and pin to this new aircraft
        _displayKey    = icao;
        _lastCycleTime = millis();
        if (mode != ScreenMode::Debug) mode = ScreenMode::Scanning;

        notifier.notifyDetection(aircraft, cfg);
        if (aircraft.isEmergencySquawk()) notifier.notifyEmergencySquawk(aircraft, cfg);
    }

    // Remove aircraft that no longer appear in the response
    for (auto it = _activeAircraft.begin(); it != _activeAircraft.end(); ) {
        if (!seen.count(it->first)) {
            it = _activeAircraft.erase(it);
        } else {
            ++it;
        }
    }

    // If the aircraft we were showing left, snap to the next one
    if (!_activeAircraft.empty() && !_activeAircraft.count(_displayKey)) {
        _displayKey    = _activeAircraft.begin()->first;
        _lastCycleTime = millis();
    }
}
