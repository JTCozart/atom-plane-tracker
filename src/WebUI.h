#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "Config.h"
#include "AircraftStore.h"
#include "Display.h"
#include "AppState.h"

class WebUI {
public:
    WebUI(Config& cfg, AircraftStore& store, Display& display)
        : _cfg(cfg), _store(store), _display(display) {}

    // Set up HTTP handlers and optionally start SoftAP.
    // mode is stored by pointer so handlers can read the current screen state.
    void begin(ScreenMode& mode, bool isSetupMode);

    // Call from loop() — dispatches pending HTTP requests.
    void processRequests();

    bool isInSetupMode() const           { return _inSetupMode; }
    const String& ipAddress() const      { return _ipAddress; }
    void setIPAddress(const String& ip)  { _ipAddress = ip; }

private:
    WebServer    _server{80};
    bool         _inSetupMode = false;
    String       _ipAddress   = "0.0.0.0";
    ScreenMode*  _screenMode  = nullptr;

    Config&        _cfg;
    AircraftStore& _store;
    Display&       _display;

    void handleRoot();
    void handleSave();
    void handleClear();
    void handleScreen();

    static String rgb565ToCss(uint16_t c);
};
