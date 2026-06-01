#pragma once
#include <ArduinoJson.h>
#include "Config.h"

class IAircraftSource {
public:
    virtual ~IAircraftSource() = default;
    virtual bool fetch(const Config& cfg, JsonDocument& out) = 0;
    virtual int  lastResponseCode()    const = 0;
    virtual int  consecutiveFailures() const = 0;
};
