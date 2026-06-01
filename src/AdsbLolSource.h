#pragma once
#include "IAircraftSource.h"

class AdsbLolSource : public IAircraftSource {
public:
    bool fetch(const Config& cfg, JsonDocument& out) override;
    int  lastResponseCode()    const override { return _lastResponseCode; }
    int  consecutiveFailures() const override { return _consecutiveFailures; }

private:
    static constexpr int         kHttpTimeoutMs = 8000;
    static constexpr const char* kUrlFormat     =
        "https://api.adsb.lol/v2/lat/%.6f/lon/%.6f/dist/%.1f";

    int _lastResponseCode    = 0;
    int _consecutiveFailures = 0;
};
