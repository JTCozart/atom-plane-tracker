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

void AircraftStore::addToHistory(const Ac& ac) {
    int slots = (_histCount < 5) ? _histCount : 4;
    for (int i = slots; i > 0; i--) _histLog[i] = _histLog[i - 1];
    _histLog[0] = ac;
    if (_histCount < 5) _histCount++;
    _histIdx = 0;  // always show newest on next HIST visit
}

// ── Active aircraft accessors ─────────────────────────────────────────────────

bool AircraftStore::hasActive() const {
    return !_inRadius.empty();
}

int AircraftStore::activeCount() const {
    return (int)_inRadius.size();
}

const Ac* AircraftStore::displayAircraft() const {
    if (_inRadius.empty()) return nullptr;
    auto it = _inRadius.find(_liveKey);
    if (it == _inRadius.end()) it = _inRadius.begin();
    return &it->second;
}

void AircraftStore::advanceCycle() {
    if (_inRadius.empty()) return;
    auto it = _inRadius.find(_liveKey);
    if (it == _inRadius.end() || ++it == _inRadius.end())
        it = _inRadius.begin();
    _liveKey     = it->first;
    _cycleTimer  = millis();
}

// ── Fetch & update ────────────────────────────────────────────────────────────

void AircraftStore::fetch(const Config& cfg, Notifier& notifier, ScreenMode& mode) {
    char url[128];
    snprintf(url, sizeof(url), API_FMT,
             cfg.lat, cfg.lon, (double)cfg.radius);

    WiFiClientSecure client;
    client.setInsecure();  // skip cert validation on embedded
    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(8000);

    int httpCode = http.GET();
    _lastHttpCode = httpCode;
    if (httpCode != 200) { http.end(); return; }

    JsonDocument doc;
    if (deserializeJson(doc, http.getStream())) { http.end(); return; }
    http.end();

    // Rebuild debug lines from raw response
    JsonArray acArr = doc["ac"].as<JsonArray>();
    _lastAcTotal = acArr.size();
    _debugLines.clear();
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
        _debugLines.push_back(buf);

        snprintf(buf, sizeof(buf), " cat:%s mil:%c %5.0fft",
                 cat.c_str(), mil ? 'Y' : 'N', alt);
        _debugLines.push_back(buf);

        if (own.length()) {
            _debugLines.push_back(String(" ") + own);
        }
        _debugLines.push_back("---");
    }
    if (_debugLines.empty()) _debugLines.push_back("No aircraft");

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

        if (_inRadius.count(icao)) {
            // Refresh position/speed data so ETA stays accurate
            Ac& existing = _inRadius[icao];
            existing.alt       = alt;
            existing.lat       = lat;
            existing.lon       = lon;
            existing.gs        = gs;
            existing.track     = track;
            existing.lastFixMs = millis();
            continue;
        }

        bool milFlag = (plane["mil"] | 0) != 0 || ((plane["dbFlags"] | 0) & 1) != 0;

        const char* cs_raw = plane["flight"] | "";
        String callsign = cs_raw;
        callsign.trim();

        String type  = plane["t"]        | "";
        String owner = plane["ownOp"]    | "";
        String cat   = plane["category"] | "";

        AcClass cls = Ac::classify(callsign, owner, milFlag, cat);

        Ac ac = { icao, callsign, type, owner, alt, lat, lon, gs, track, millis(), cls };
        _inRadius[icao] = ac;
        _cnt[cls]++;
        addToHistory(ac);

        // Interrupt to live view and pin to this new aircraft
        _liveKey     = icao;
        _cycleTimer  = millis();
        if (mode != SCR_DEBUG) mode = SCR_SCAN;

        notifier.send(ac, cfg);
    }

    // Remove aircraft that no longer appear in the response
    for (auto it = _inRadius.begin(); it != _inRadius.end(); ) {
        if (!seen.count(it->first)) {
            it = _inRadius.erase(it);
        } else {
            ++it;
        }
    }

    // If the aircraft we were showing left, snap to the next one
    if (!_inRadius.empty() && !_inRadius.count(_liveKey)) {
        _liveKey    = _inRadius.begin()->first;
        _cycleTimer = millis();
    }
}
