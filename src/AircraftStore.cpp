#include "AircraftStore.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <map>
#include <set>

// adsb.lol — free, no API key, ADSBExchange v2 format, radius in NM
// Fallback: "https://api.adsb.one/v2/point/%.6f/%.6f/%.1f"
static const char* API_FMT = "https://api.adsb.lol/v2/lat/%.6f/lon/%.6f/dist/%.1f";

// ── History ───────────────────────────────────────────────────────────────────

void AircraftStore::recordInHistory(const Aircraft& aircraft) {
    int slots = (_historyCount < 5) ? _historyCount : 4;
    for (int i = slots; i > 0; i--) _history[i] = _history[i - 1];
    _history[0] = aircraft;
    if (_historyCount < 5) _historyCount++;
    _historyIndex = 0;  // always show newest on next HIST visit
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

    int httpCode = http.GET();
    _lastResponseCode = httpCode;
    if (httpCode != 200) { http.end(); return; }

    JsonDocument doc;
    if (deserializeJson(doc, http.getStream())) { http.end(); return; }
    http.end();

    // Rebuild debug lines from raw response
    JsonArray acArr = doc["ac"].as<JsonArray>();
    _lastAircraftCount = acArr.size();
    _apiResponseLines.clear();
    for (JsonObject plane : acArr) {
        String icao = plane["hex"]      | "??????";  icao.toUpperCase();
        String cs   = plane["flight"]   | "";        cs.trim();
        String reg  = plane["r"]        | "";
        String type = plane["t"]        | "??";
        String cat  = plane["category"] | "?";
        String own  = plane["ownOp"]    | "";
        float  alt  = plane["alt_baro"] | 0.0f;
        bool   mil  = (plane["mil"] | 0) != 0 || ((plane["dbFlags"] | 0) & 1) != 0;

        char buf[48];
        snprintf(buf, sizeof(buf), "%s %s %s", icao.c_str(),
                 cs.length() ? cs.c_str() : reg.c_str(), type.c_str());
        _apiResponseLines.push_back(buf);

        snprintf(buf, sizeof(buf), " cat:%s mil:%c %5.0fft",
                 cat.c_str(), mil ? 'Y' : 'N', alt);
        _apiResponseLines.push_back(buf);
        _apiResponseLines.push_back("---");
    }
    if (_apiResponseLines.empty()) _apiResponseLines.push_back("No aircraft");

    // Don't reset scroll if user is actively viewing debug
    // (scroll is owned by the caller / main.cpp — nothing to reset here)

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

        if (_activeAircraft.count(icao)) {
            // Refresh position/speed data so ETA stays accurate
            Aircraft& existing       = _activeAircraft[icao];
            existing.altitude         = alt;
            existing.latitude         = lat;
            existing.longitude        = lon;
            existing.groundSpeed      = gs;
            existing.trackDegrees     = track;
            existing.positionTimestamp = millis();
            continue;
        }

        bool milFlag = (plane["mil"] | 0) != 0 || ((plane["dbFlags"] | 0) & 1) != 0;

        const char* cs_raw = plane["flight"] | "";
        String callsign = cs_raw;
        callsign.trim();

        String type  = plane["t"]        | "";
        String owner = plane["ownOp"]    | "";
        String cat   = plane["category"] | "";

        AircraftClass classification = Aircraft::classify(callsign, owner, milFlag, cat);

        Aircraft aircraft = { icao, callsign, type, owner, alt, lat, lon, gs, track, millis(), classification };
        _activeAircraft[icao] = aircraft;
        _detectionCounts[toIndex(classification)]++;
        recordInHistory(aircraft);

        // Interrupt to live view and pin to this new aircraft
        _displayKey    = icao;
        _lastCycleTime = millis();
        if (mode != ScreenMode::Debug) mode = ScreenMode::Scanning;

        notifier.notifyDetection(aircraft, cfg);
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
