#include "Aircraft.h"
#include <cmath>

const char* aircraftClassName(AircraftClass cls) {
    switch (cls) {
        case AircraftClass::Military:   return "MILITARY";
        case AircraftClass::Medevac:    return "MEDEVAC";
        case AircraftClass::Commercial: return "COMMERCIAL";
        default:                        return "PRIVATE";
    }
}

// ── Classification helpers ────────────────────────────────────────────────────

// FAA LIFEGUARD prefix is a regulated designation — safe to match on callsign.
// All other classification comes from the API's own fields.
static const char* MEDVAC_CS[] = {
    "LIFEGRD", "MEDVAC", "AIRLIFE", nullptr
};
static const char* MEDVAC_OP[] = {
    "AIR LIFE", "AIR METHODS", "PHI AIR", "METRO AVIA",
    "OMNIFLIGHT", "GUARDIAN FL", nullptr
};

static bool startsWithAny(const String& s, const char** list) {
    for (; *list; list++) if (s.startsWith(*list)) return true;
    return false;
}
static bool containsAny(const String& s, const char** list) {
    for (; *list; list++) if (s.indexOf(*list) >= 0) return true;
    return false;
}

// ── Aircraft::classify ────────────────────────────────────────────────────────

AircraftClass Aircraft::classify(const String& callsign, const String& owner,
                                  bool milFlag, const String& category) {
    // Military: API flags only — no callsign guessing to avoid false positives
    if (milFlag) return AircraftClass::Military;

    String upperCallsign = callsign; upperCallsign.toUpperCase();
    String upperOwner    = owner;    upperOwner.toUpperCase();

    // Medevac: FAA LIFEGUARD callsign prefix or known operator
    if (startsWithAny(upperCallsign, MEDVAC_CS)) return AircraftClass::Medevac;
    if (containsAny(upperOwner, MEDVAC_OP))      return AircraftClass::Medevac;

    // Commercial: API category A3=large, A4=high-vortex (B757), A5=heavy
    if (category == "A3" || category == "A4" || category == "A5") return AircraftClass::Commercial;

    // Commercial: standard ICAO airline designator pattern (e.g. DAL123, UAL456)
    if (upperCallsign.length() >= 5 && upperCallsign.length() <= 8 &&
        isAlpha(upperCallsign[0]) && isAlpha(upperCallsign[1]) &&
        isAlpha(upperCallsign[2]) && isDigit(upperCallsign[3]))
        return AircraftClass::Commercial;

    return AircraftClass::Private;
}

// ── Aircraft::etaSeconds ──────────────────────────────────────────────────────

// Returns seconds until the aircraft exits the query radius, or -1 if unknown.
// Steps the aircraft position forward 5-second increments along its track until
// it leaves the circle, capping at 60 minutes to avoid infinite loops.
int Aircraft::etaSeconds(double queryLatitude, double queryLongitude, float queryRadius) const {
    if (groundSpeed < 5.0f || latitude == 0.0f) return -1;

    const float R_NM           = queryRadius;
    const float DEG2RAD        = M_PI / 180.0f;
    const float NM_PER_DEG_LAT = 60.0f;

    float curLat = latitude;
    float curLon = longitude;
    float trackRad = trackDegrees * DEG2RAD;
    float speedNmPerSec = groundSpeed / 3600.0f;

    for (int sec = 0; sec <= 3600; sec += 5) {
        float dLat = cosf(trackRad) * speedNmPerSec * 5.0f / NM_PER_DEG_LAT;
        float dLon = sinf(trackRad) * speedNmPerSec * 5.0f /
                     (NM_PER_DEG_LAT * cosf(curLat * DEG2RAD));
        curLat += dLat;
        curLon += dLon;

        float dlat = (curLat - (float)queryLatitude) * NM_PER_DEG_LAT;
        float dlon = (curLon - (float)queryLongitude) * NM_PER_DEG_LAT * cosf((float)queryLatitude * DEG2RAD);
        float dist = sqrtf(dlat * dlat + dlon * dlon);

        if (dist > R_NM) return sec;
    }
    return -1;  // still inside after 60 min
}
