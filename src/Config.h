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

    // Load settings from NVS; falls back to secrets.h macro defaults.
    void load();
};
