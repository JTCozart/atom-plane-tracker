#include <M5Unified.h>
#include <WiFi.h>
#include "Aircraft.h"
#include "AircraftJsonApi.h"
#include "AircraftPersistence.h"
#include "AircraftStore.h"
#include "AdsbLolSource.h"
#include "Config.h"
#include "Display.h"
#include "FetchEffect.h"
#include "Notifier.h"
#include "OtaUpdater.h"
#include "ScreenController.h"
#include "WebUI.h"
#include "AnalyticsReporter.h"
#include "secrets.h"
#include <esp_ota_ops.h>

// ── Global instances (declared in dependency order) ───────────────────────────

static Config              config;
static AircraftPersistence persistence;
static AdsbLolSource       adsbSource;
static AircraftStore       store(adsbSource, persistence);
static Display             display;
static Notifier            notifier;
static OtaUpdater          ota;
static ScreenController    screenController;
static AircraftJsonApi     api(store, config, display);
static WebUI               webUI(config, store, display, ota, api, screenController);

// Button timing state
static uint32_t lastInteractionTime = 0;
static uint32_t buttonPressTime     = 0;
static bool     longPressHandled    = false;
static constexpr uint32_t kIdleTimeoutMs       = 30000;
static constexpr uint32_t kLongPressDurationMs = 800;

// ── WiFi ──────────────────────────────────────────────────────────────────────

static bool connectToWifi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.ssid, config.password);

    for (int frame = 0; frame < 80 && WiFi.status() != WL_CONNECTED; frame++) {
        display.showSplash(frame);
        delay(250);
    }

    if (WiFi.status() == WL_CONNECTED) {
        webUI.setIPAddress(WiFi.localIP().toString());
        webUI.begin(false);
        return true;
    }
    return false;
}

// ── Poll interval validation ──────────────────────────────────────────────────

static void validatePollInterval() {
    if (config.pollIntervalMs >= Config::kMinPollIntervalMs) return;

    const int kErrorTimeoutSeconds = 15;
    uint32_t deadline = millis() + (uint32_t)kErrorTimeoutSeconds * 1000;
    int lastSecond    = -1;

    while (millis() < deadline) {
        M5.update();
        int remaining = (int)((deadline - millis()) / 1000);

        if (remaining != lastSecond) {
            lastSecond = remaining;
            display.showPollIntervalError(config.pollIntervalMs,
                                          Config::kMinPollIntervalMs,
                                          remaining);
        }

        if (M5.BtnA.wasPressed()) {
            config.savePollInterval(Config::kMinPollIntervalMs);
            ESP.restart();
        }
        delay(50);
    }

    config.pollIntervalMs = Config::kMinPollIntervalMs;
}

// ── Render ────────────────────────────────────────────────────────────────────

static void render() {
    display.setUpdateAvailable(ota.hasUpdate());
    switch (screenController.current()) {
        case ScreenMode::Scanning:
            if (store.hasActiveAircraft()) {
                const Aircraft* ac = store.currentAircraft();
                if (ac) {
                    int eta = ac->etaSeconds(config.latitude, config.longitude, config.radius);
                    display.showLiveAircraft(*ac, eta, config.latitude, config.longitude);
                }
            } else if (store.consecutiveFailures() > 0) {
                display.showLostConnection(store.consecutiveFailures());
            } else {
                display.showScanning();
            }
            break;
        case ScreenMode::History:
            if (store.historyCount() > 0)
                display.showHistoryAircraft(store.historyAt(store.historyIndex()),
                                            store.historyIndex(),
                                            store.historyCount(),
                                            config.latitude, config.longitude);
            else
                display.showScanning();
            break;
        case ScreenMode::Summary:
            display.showSummary(store.detectionCount(AircraftClass::Military),
                                store.detectionCount(AircraftClass::Medevac),
                                store.detectionCount(AircraftClass::Commercial),
                                store.detectionCount(AircraftClass::Private));
            break;
        case ScreenMode::Radar:
            display.showRadar(store.activeAircraft(),
                              config.latitude, config.longitude, config.radius);
            break;
        case ScreenMode::Debug:
            display.showDebug(store.apiResponseLines(), screenController.debugScrollOffset(), {
                store.lastResponseCode(),
                store.lastAircraftCount(),
                webUI.ipAddress(),
                millis() / 1000,
                OtaUpdater::currentVersion()
            });
            break;
        case ScreenMode::PinDisplay:
            display.showPinDisplay(screenController.pendingPin(),
                                   screenController.pendingPinTitle());
            break;
    }
}

// ── Loop helpers ──────────────────────────────────────────────────────────────

static void handleWebControlChange() {
    if (!screenController.consumeWebControlChange()) return;
    lastInteractionTime = millis();
    render();
}

static void handleIdleTimeout() {
    ScreenMode mode = screenController.current();
    bool isIdleableMode = (mode == ScreenMode::History || mode == ScreenMode::Summary);
    if (!isIdleableMode) return;
    if (millis() - lastInteractionTime < kIdleTimeoutMs) return;
    screenController.setMode(ScreenMode::Scanning);
    render();
}

static void handleDebugShortPress() {
    int totalLines = (int)store.apiResponseLines().size();
    if (screenController.handleDebugShortPress(totalLines, Display::kVisibleLineCount))
        notifier.sendTestNotification(config, display);
    render();
}

static void handleNormalShortPress() {
    if (screenController.handleShortPress(store)) store.clearCounts();
    if (screenController.consumePinCancelled()) webUI.cancelPinFromDevice();
    render();
}

static void handleButtonInput() {
    if (M5.BtnA.wasPressed()) {
        buttonPressTime     = millis();
        longPressHandled    = false;
        lastInteractionTime = millis();
    }

    if (M5.BtnA.isPressed() && !longPressHandled &&
        (millis() - buttonPressTime >= kLongPressDurationMs)) {
        longPressHandled = true;
        screenController.toggleDebug();
        render();
        return;
    }

    if (!M5.BtnA.wasReleased() || longPressHandled) return;

    lastInteractionTime = millis();
    if (screenController.isDebugMode())
        handleDebugShortPress();
    else
        handleNormalShortPress();
}

static void handleAutoRefresh() {
    static uint32_t lastEtaRedraw   = 0;
    static uint32_t lastScanRefresh = 0;

    if (screenController.current() == ScreenMode::Scanning && store.hasActiveAircraft()) {
        if (store.activeAircraftCount() > 1 &&
            millis() - store.lastCycleTime() >= AircraftStore::kCycleIntervalMs) {
            store.cycleToNextAircraft();
            render();
            lastEtaRedraw = millis();
        } else if (millis() - lastEtaRedraw >= 1000) {
            lastEtaRedraw = millis();
            render();
        }
    }

    if (screenController.current() == ScreenMode::Scanning && !store.hasActiveAircraft() &&
        store.consecutiveFailures() == 0 &&
        millis() - lastScanRefresh >= 50) {
        lastScanRefresh = millis();
        display.showScanning();
    }
}

static void handlePoll() {
    static uint32_t lastPoll = 0;
    if (millis() - lastPoll < config.pollIntervalMs) return;
    lastPoll = millis();
    FetchEffect effect = store.fetch(config, notifier);
    if (effect == FetchEffect::NewAircraftDetected)
        screenController.onNewAircraftDetected();
    render();
}

static void handleAnalyticsHeartbeat() {
    static constexpr uint32_t kHeartbeatIntervalMs = 60 * 60 * 1000; // 1 hour
    static uint32_t lastHeartbeat = 0;
    if (millis() - lastHeartbeat < kHeartbeatIntervalMs) return;
    lastHeartbeat = millis();
    AnalyticsReporter::reportHeartbeat();
}

// ── Entry points ──────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(2000); // allow USB CDC to re-enumerate after reset

    const esp_partition_t* running = esp_ota_get_running_partition();
    Serial.printf("[BOOT] partition: %s  offset: 0x%X\n",
                  running ? running->label : "unknown",
                  running ? running->address : 0);
    Serial.printf("[BOOT] firmware: %s\n", OtaUpdater::currentVersion());
    Serial.printf("[BOOT] reset reason: %d\n", (int)esp_reset_reason());

    auto m5cfg = M5.config();
    M5.begin(m5cfg);

    display.begin();
    config.load();
    store.loadCountsFromNVS();
    store.loadHistoryFromNVS();
    validatePollInterval();

    if (config.isUnconfigured() || !connectToWifi()) {
        webUI.begin(true);   // AP mode: starts SoftAP + web server
        display.showSetupMode();
        return;  // loop() handles AP from here
    }

    AnalyticsReporter::reportBoot();

    display.showScanning();
    FetchEffect effect = store.fetch(config, notifier);
    if (effect == FetchEffect::NewAircraftDetected)
        screenController.onNewAircraftDetected();
    render();
}

void loop() {
    if (webUI.isInSetupMode()) {
        webUI.processRequests();
        return;
    }

    M5.update();
    webUI.processRequests();

    handleWebControlChange();
    handleIdleTimeout();
    handleButtonInput();
    if (screenController.update(millis())) render();
    handleAutoRefresh();
    handlePoll();
    handleAnalyticsHeartbeat();

    if (!webUI.isInSetupMode() && ota.isDue()) {
        if (ota.check()) notifier.notifyUpdate(ota.latestVersion(), config);
    }
}
