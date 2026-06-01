#include "AircraftJsonApi.h"
#include "AircraftStore.h"
#include "Config.h"
#include "Display.h"
#include "Aircraft.h"

AircraftJsonApi::AircraftJsonApi(const AircraftStore& store, const Config& cfg, const Display& display)
    : _store(store), _cfg(cfg), _display(display) {}

String AircraftJsonApi::jsonEscape(const String& s) {
    String result = s;
    result.replace("\"", "'");
    return result;
}

String AircraftJsonApi::serializeActiveAircraft() const {
    String json = "{\"aircraft\":[";
    bool first = true;
    for (const auto& kv : _store.activeAircraft()) {
        const Aircraft& ac = kv.second;
        if (!first) json += ",";
        first = false;

        String bg       = Display::rgb565ToCss(_display.backgroundColorFor(ac.classification));
        String fg       = Display::rgb565ToCss(_display.foregroundColorFor(ac.classification));
        String callsign = ac.callsign.length() ? ac.callsign
                        : ac.registration.length() ? ac.registration : ac.icao;
        float       dist    = ac.distanceNm(_cfg.latitude, _cfg.longitude);
        const char* compass = Aircraft::compassPoint(ac.bearingDeg(_cfg.latitude, _cfg.longitude));
        bool        inbound = ac.isApproaching(_cfg.latitude, _cfg.longitude);

        char buf[320];
        snprintf(buf, sizeof(buf),
            "{\"icao\":\"%s\",\"callsign\":\"%s\",\"type\":\"%s\","
            "\"lat\":%.6f,\"lon\":%.6f,\"alt\":%.0f,\"track\":%.0f,\"speed\":%.0f,"
            "\"sqwk\":\"%s\",\"bg\":\"%s\",\"fg\":\"%s\","
            "\"dist\":\"%.1f\",\"compass\":\"%s\",\"inbound\":%s}",
            jsonEscape(ac.icao).c_str(), jsonEscape(callsign).c_str(), jsonEscape(ac.type).c_str(),
            ac.latitude, ac.longitude, ac.altitude, ac.trackDegrees, ac.groundSpeed,
            jsonEscape(ac.squawk).c_str(), bg.c_str(), fg.c_str(),
            dist, compass, inbound ? "true" : "false");
        json += buf;
    }
    json += "],\"center\":{\"lat\":" + String(_cfg.latitude,  6)
          + ",\"lon\":"              + String(_cfg.longitude, 6) + "}"
          + ",\"radiusM\":"          + String((int)(_cfg.radius * 1852.0f)) + "}";
    return json;
}

String AircraftJsonApi::serializeHistory() const {
    String json = "{\"aircraft\":[";
    for (int i = 0; i < _store.webHistoryCount(); i++) {
        const Aircraft& ac = _store.historyAt(i);
        if (i > 0) json += ",";

        String bg       = Display::rgb565ToCss(_display.backgroundColorFor(ac.classification));
        String fg       = Display::rgb565ToCss(_display.foregroundColorFor(ac.classification));
        String callsign = ac.callsign.length() ? ac.callsign
                        : ac.registration.length() ? ac.registration : ac.icao;
        String trackUrl = "https://globe.adsbexchange.com/?icao=" + ac.icao;

        char buf[320];
        snprintf(buf, sizeof(buf),
            "{\"callsign\":\"%s\",\"type\":\"%s\",\"alt\":%.0f,\"speed\":%.0f,"
            "\"sqwk\":\"%s\",\"cls\":\"%s\",\"bg\":\"%s\",\"fg\":\"%s\",\"url\":\"%s\"}",
            jsonEscape(callsign).c_str(), jsonEscape(ac.type).c_str(),
            ac.altitude, ac.groundSpeed,
            jsonEscape(ac.squawk).c_str(), aircraftClassName(ac.classification),
            bg.c_str(), fg.c_str(), trackUrl.c_str());
        json += buf;
    }
    json += "]}";
    return json;
}
