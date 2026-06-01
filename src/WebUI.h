#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "Config.h"
#include "AircraftStore.h"
#include "Display.h"
#include "AppState.h"
#include "OtaUpdater.h"

class WebUI {
public:
    WebUI(Config& cfg, AircraftStore& store, Display& display, OtaUpdater& ota)
        : _cfg(cfg), _store(store), _display(display), _ota(ota) {}

    // Set up HTTP handlers and optionally start SoftAP.
    // mode is stored by pointer so handlers can read the current screen state.
    void begin(ScreenMode& mode, bool isSetupMode);

    // Call from loop() - dispatches pending HTTP requests.
    void processRequests();

    bool isInSetupMode() const           { return _inSetupMode; }
    const String& ipAddress() const      { return _ipAddress; }
    void setIPAddress(const String& ip)  { _ipAddress = ip; }

    // Returns true (and clears the flag) if a /control request changed the screen mode.
    bool consumeControlChange() { bool v = _controlChanged; _controlChanged = false; return v; }

private:
    WebServer    _server{80};
    bool         _inSetupMode = false;
    String       _ipAddress   = "0.0.0.0";
    ScreenMode*  _screenMode    = nullptr;
    bool         _controlChanged = false;

    Config&        _cfg;
    AircraftStore& _store;
    Display&       _display;
    OtaUpdater&    _ota;

    void handleRoot();
    void handleSave();
    void handleClear();
    void handleControl();
    void handleScreen();
    void handleNotifyTest();
    void handleNtfyStats();
    void handleApiTest();
    void handleOtaCheck();
    void handleOtaUpdate();
    void handleAircraft();
    void handleHistory();
    void handleClearSummary();

    String buildScreenDiv();
    static String buildRebootPage(const String& heading, const String& subtext);
    static String rgb565ToCss(uint16_t c);
};
