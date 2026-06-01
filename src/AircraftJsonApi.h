#pragma once
#include <Arduino.h>

class AircraftStore;
class Config;
class Display;

class AircraftJsonApi {
public:
    AircraftJsonApi(const AircraftStore& store, const Config& cfg, const Display& display);

    String serializeActiveAircraft() const;
    String serializeHistory() const;

private:
    const AircraftStore& _store;
    const Config&        _cfg;
    const Display&       _display;

    static String jsonEscape(const String& s);
};
