#include "AircraftPersistence.h"
#include <Preferences.h>

const char* const AircraftPersistence::kNvsNamespace = "plantracker";
const char* const AircraftPersistence::kCountKeys[AircraftPersistence::kClassCount] = {
    "sumMil", "sumMed", "sumComm", "sumPriv"
};

namespace {
    struct StoredAircraft {
        char    icao[8];
        char    callsign[10];
        char    registration[10];
        char    type[6];
        char    owner[33];
        char    squawk[6];
        float   altitude;
        float   latitude;
        float   longitude;
        float   groundSpeed;
        float   trackDegrees;
        uint8_t classification;
        uint8_t _pad[3];  // align to 4 bytes
    };
}

void AircraftPersistence::loadCounts(int* counts) const {
    Preferences prefs;
    prefs.begin(kNvsNamespace, true);
    for (int i = 0; i < kClassCount; i++)
        counts[i] = prefs.getInt(kCountKeys[i], 0);
    prefs.end();
}

void AircraftPersistence::saveCounts(const int* counts) const {
    Preferences prefs;
    prefs.begin(kNvsNamespace, false);
    for (int i = 0; i < kClassCount; i++)
        prefs.putInt(kCountKeys[i], counts[i]);
    prefs.end();
}

int AircraftPersistence::loadHistory(Aircraft* history) const {
    Preferences prefs;
    prefs.begin(kNvsNamespace, true);
    int count = min((int)prefs.getInt("histCount", 0), kWebHistoryMax);
    if (count > 0) {
        StoredAircraft buf[kWebHistoryMax] = {};
        prefs.getBytes("histData", buf, sizeof(StoredAircraft) * count);
        for (int i = 0; i < count; i++) {
            Aircraft& aircraft         = history[i];
            aircraft.icao              = buf[i].icao;
            aircraft.callsign          = buf[i].callsign;
            aircraft.registration      = buf[i].registration;
            aircraft.type              = buf[i].type;
            aircraft.owner             = buf[i].owner;
            aircraft.squawk            = buf[i].squawk;
            aircraft.altitude          = buf[i].altitude;
            aircraft.latitude          = buf[i].latitude;
            aircraft.longitude         = buf[i].longitude;
            aircraft.groundSpeed       = buf[i].groundSpeed;
            aircraft.trackDegrees      = buf[i].trackDegrees;
            aircraft.positionTimestamp = 0;  // invalid after reboot — ETA shows --:--
            aircraft.classification    = static_cast<AircraftClass>(buf[i].classification);
        }
    }
    prefs.end();
    return count;
}

void AircraftPersistence::saveHistory(const Aircraft* history, int count) const {
    StoredAircraft buf[kWebHistoryMax] = {};
    for (int i = 0; i < count; i++) {
        const Aircraft& aircraft = history[i];
        strncpy(buf[i].icao,         aircraft.icao.c_str(),         sizeof(buf[i].icao)         - 1);
        strncpy(buf[i].callsign,     aircraft.callsign.c_str(),     sizeof(buf[i].callsign)     - 1);
        strncpy(buf[i].registration, aircraft.registration.c_str(), sizeof(buf[i].registration) - 1);
        strncpy(buf[i].type,         aircraft.type.c_str(),         sizeof(buf[i].type)         - 1);
        strncpy(buf[i].owner,        aircraft.owner.c_str(),        sizeof(buf[i].owner)        - 1);
        strncpy(buf[i].squawk,       aircraft.squawk.c_str(),       sizeof(buf[i].squawk)       - 1);
        buf[i].altitude       = aircraft.altitude;
        buf[i].latitude       = aircraft.latitude;
        buf[i].longitude      = aircraft.longitude;
        buf[i].groundSpeed    = aircraft.groundSpeed;
        buf[i].trackDegrees   = aircraft.trackDegrees;
        buf[i].classification = (uint8_t)toIndex(aircraft.classification);
    }
    Preferences prefs;
    prefs.begin(kNvsNamespace, false);
    prefs.putInt  ("histCount", count);
    prefs.putBytes("histData",  buf, sizeof(StoredAircraft) * count);
    prefs.end();
}
