#pragma once
#include <Arduino.h>

struct Config {
    char     ssid[64];
    char     password[64];
    double   latitude;
    double   longitude;
    float    radius;
    uint32_t pollIntervalMs;
    char     notifyToken[128];
    char     notifyTopic[64];
    char     notifyClassFilter[32];
    bool     notifyUpdates;
    bool     notifyEmergencySquawk;
    bool     notifyPoi;             // Notify POI aircraft; overrides class filter
    char     poiTypes[256];         // CSV of ICAO type codes for POI filter
    bool     poiEnabled;            // Show only POI types on device/map/radar

    static constexpr uint32_t    kMinPollIntervalMs = 10000;
    static constexpr const char* kSetupSentinel     = "SETUP";

    bool isUnconfigured()   const { return strcmp(ssid, kSetupSentinel) == 0; }
    bool poiDisplayActive() const { return poiEnabled && poiTypes[0] != '\0'; }
    bool poiNotifyActive()  const { return notifyPoi  && poiTypes[0] != '\0'; }

    // Load settings from NVS; falls back to secrets.h macro defaults.
    void load();

    // Persist the poll interval to NVS and update the in-memory value.
    void savePollInterval(uint32_t ms);
};
