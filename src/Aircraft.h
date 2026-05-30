#pragma once
#include <Arduino.h>

enum class AircraftClass { Military = 0, Medevac, Commercial, Private };

const char* aircraftClassName(AircraftClass cls);

inline int toIndex(AircraftClass cls) { return static_cast<int>(cls); }

struct Aircraft {
    String   icao;
    String   callsign;
    String   type;
    String   owner;
    float    altitude;
    float    latitude;
    float    longitude;
    float    groundSpeed;      // knots
    float    trackDegrees;     // true track, degrees
    uint32_t positionTimestamp; // millis() when lat/lon/gs/track were last updated
    AircraftClass classification;

    // Returns the AircraftClass for the given aircraft attributes
    static AircraftClass classify(const String& callsign, const String& owner,
                                  bool milFlag, const String& category);

    // Returns seconds until aircraft exits query circle, or -1 if unknown.
    // Takes query params explicitly (no globals).
    int etaSeconds(double queryLatitude, double queryLongitude, float queryRadius) const;
};
