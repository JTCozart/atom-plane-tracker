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
    void begin(ScreenMode& mode, bool apMode);

    // Call from loop() — dispatches pending HTTP requests.
    void handle();

    bool isAPMode() const      { return _apMode; }
    const String& deviceIP() const { return _deviceIP; }
    void setDeviceIP(const String& ip) { _deviceIP = ip; }

private:
    WebServer    _server{80};
    bool         _apMode   = false;
    String       _deviceIP = "0.0.0.0";
    ScreenMode*  _mode     = nullptr;

    Config&        _cfg;
    AircraftStore& _store;
    Display&       _display;

    void handleRoot();
    void handleSave();
    void handleScreen();

    static String rgb565ToCss(uint16_t c);
};
