#pragma once
#include <Arduino.h>

enum class AircraftClass { Military = 0, Medevac, Commercial, Private };

const char* aircraftClassName(AircraftClass cls);  // "MILITARY", "MEDEVAC", etc.
const char* aircraftClassTag(AircraftClass cls);   // "MIL", "MEDVAC", "COMM", "PRIV"

inline int toIndex(AircraftClass cls) { return static_cast<int>(cls); }

struct Aircraft {
    String   icao;
    String   callsign;
    String   registration;  // tail number (aircraft identifier)
    String   type;
    String   owner;
    String   squawk;
    float    altitude;
    float    latitude;
    float    longitude;
    float    groundSpeed;      // knots
    float    trackDegrees;     // true track, degrees
    uint32_t positionTimestamp; // millis() when lat/lon/gs/track were last updated
    AircraftClass classification;

    static bool isEmergencySquawkCode(const String& sq) {
        return sq == "7500" || sq == "7600" || sq == "7700";
    }
    bool isEmergencySquawk() const { return isEmergencySquawkCode(squawk); }

    // Returns the AircraftClass for the given aircraft attributes
    static AircraftClass classify(const String& callsign, const String& owner,
                                  bool milFlag, const String& category);

    // Returns seconds until aircraft exits query circle, or -1 if unknown.
    int etaSeconds(double queryLatitude, double queryLongitude, float queryRadius) const;

    // Adjusts a raw etaSeconds value for elapsed time since positionTimestamp.
    // Returns clamped result (>= 0), or -1 if rawEta is -1.
    int adjustedEta(int rawEta) const;
};
