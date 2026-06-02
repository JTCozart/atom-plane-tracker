#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "AircraftJsonApi.h"
#include "Config.h"
#include "AircraftStore.h"
#include "Display.h"
#include "OtaUpdater.h"
#include "ScreenController.h"

class WebUI {
public:
    WebUI(Config& cfg, AircraftStore& store, Display& display, OtaUpdater& ota,
          AircraftJsonApi& api, ScreenController& screenController)
        : _cfg(cfg), _store(store), _display(display), _ota(ota),
          _api(api), _screenController(screenController) {}

    // Set up HTTP handlers and optionally start SoftAP.
    void begin(bool isSetupMode);

    // Call from loop() — dispatches pending HTTP requests.
    void processRequests();

    bool isInSetupMode()              const { return _inSetupMode; }
    const String& ipAddress()         const { return _ipAddress; }
    void setIPAddress(const String& ip)     { _ipAddress = ip; }

    // Called when the device button cancels an active PinDisplay.
    void cancelPinFromDevice() {
        _forgotPin            = "";
        _forgotPinAttempts    = 0;
        _lockoutCode          = "";
        _loginFailures        = 0;
        _lockoutCodeAttempts  = 0;
    }

private:
    WebServer        _server{80};
    bool             _inSetupMode = false;
    String           _ipAddress   = "0.0.0.0";
    String           _sessionToken;
    String           _forgotPin;
    String           _lockoutCode;
    uint8_t          _forgotPinAttempts{0};
    uint8_t          _loginFailures{0};
    uint8_t          _lockoutCodeAttempts{0};

    Config&           _cfg;
    AircraftStore&    _store;
    Display&          _display;
    OtaUpdater&       _ota;
    AircraftJsonApi&  _api;
    ScreenController& _screenController;

    // Returns true if the request is authenticated (or auth is not required).
    // On failure, sends a 401 JSON response and returns false.
    bool checkAuth();

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
    void handleLoginPost();
    void handleForgotPassword();
    void handlePinReset();
    void handlePinCancel();
    void handlePinStatus();

    String buildScreenDiv();
};
