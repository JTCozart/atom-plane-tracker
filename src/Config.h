#pragma once
#include <Arduino.h>

struct Config {
    char     ssid[64];
    char     pass[64];
    double   lat;
    double   lon;
    float    radius;
    uint32_t pollMs;
    char     ntfyToken[128];
    char     ntfyTopic[64];
    char     ntfyClasses[32];

    // Load settings from NVS; falls back to secrets.h macro defaults.
    void load();
};
