#pragma once
#include <Arduino.h>

enum AcClass { CLS_MIL = 0, CLS_MEDVAC, CLS_COMM, CLS_PRIV };

extern const char* AC_LABELS[4];

struct Ac {
    String   icao;
    String   callsign;
    String   type;
    String   owner;
    float    alt;
    float    lat;
    float    lon;
    float    gs;        // ground speed, knots
    float    track;     // true track, degrees
    uint32_t lastFixMs; // millis() when lat/lon/gs/track were last updated
    AcClass  cls;

    // Returns the AcClass for the given aircraft attributes
    static AcClass classify(const String& callsign, const String& owner,
                             bool milFlag, const String& cat);

    // Returns seconds until aircraft exits query circle, or -1 if unknown.
    // Takes query params explicitly (no globals).
    int etaSeconds(double queryLat, double queryLon, float queryRadius) const;
};
