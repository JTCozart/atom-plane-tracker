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

    static constexpr uint32_t kMinPollIntervalMs = 10000;

    // Load settings from NVS; falls back to secrets.h macro defaults.
    void load();

    // Persist the poll interval to NVS and update the in-memory value.
    void savePollInterval(uint32_t ms);
};
