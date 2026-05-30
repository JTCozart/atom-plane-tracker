#include "Aircraft.h"
#include <cmath>

const char* AC_LABELS[4] = { "MILITARY", "MEDEVAC", "COMMERCIAL", "PRIVATE" };

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

// ── Ac::classify ─────────────────────────────────────────────────────────────

AcClass Ac::classify(const String& callsign, const String& owner,
                     bool milFlag, const String& cat) {
    // Military: API flags only — no callsign guessing to avoid false positives
    if (milFlag) return CLS_MIL;

    String cs = callsign; cs.toUpperCase();
    String op = owner;    op.toUpperCase();

    // Medevac: FAA LIFEGUARD callsign prefix or known operator
    if (startsWithAny(cs, MEDVAC_CS)) return CLS_MEDVAC;
    if (containsAny(op, MEDVAC_OP))   return CLS_MEDVAC;

    // Commercial: API category A3=large, A4=high-vortex (B757), A5=heavy
    if (cat == "A3" || cat == "A4" || cat == "A5") return CLS_COMM;

    // Commercial: standard ICAO airline designator pattern (e.g. DAL123, UAL456)
    if (cs.length() >= 5 && cs.length() <= 8 &&
        isAlpha(cs[0]) && isAlpha(cs[1]) && isAlpha(cs[2]) && isDigit(cs[3]))
        return CLS_COMM;

    return CLS_PRIV;
}

// ── Ac::etaSeconds ────────────────────────────────────────────────────────────

// Returns seconds until the aircraft exits the query radius, or -1 if unknown.
// Steps the aircraft position forward 5-second increments along its track until
// it leaves the circle, capping at 60 minutes to avoid infinite loops.
int Ac::etaSeconds(double queryLat, double queryLon, float queryRadius) const {
    if (gs < 5.0f || lat == 0.0f) return -1;

    const float R_NM           = queryRadius;
    const float DEG2RAD        = M_PI / 180.0f;
    const float NM_PER_DEG_LAT = 60.0f;

    float curLat = lat;
    float curLon = lon;
    float trackRad = track * DEG2RAD;
    float speedNmPerSec = gs / 3600.0f;

    for (int sec = 0; sec <= 3600; sec += 5) {
        float dLat = cosf(trackRad) * speedNmPerSec * 5.0f / NM_PER_DEG_LAT;
        float dLon = sinf(trackRad) * speedNmPerSec * 5.0f /
                     (NM_PER_DEG_LAT * cosf(curLat * DEG2RAD));
        curLat += dLat;
        curLon += dLon;

        float dlat = (curLat - (float)queryLat) * NM_PER_DEG_LAT;
        float dlon = (curLon - (float)queryLon) * NM_PER_DEG_LAT * cosf((float)queryLat * DEG2RAD);
        float dist = sqrtf(dlat * dlat + dlon * dlon);

        if (dist > R_NM) return sec;
    }
    return -1;  // still inside after 60 min
}
