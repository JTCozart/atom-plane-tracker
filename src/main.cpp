#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "AppState.h"
#include "Aircraft.h"
#include "Config.h"
#include "Display.h"
#include "Notifier.h"
#include "AircraftStore.h"
#include "OtaUpdater.h"
#include "WebUI.h"
#include "secrets.h"
#include <esp_ota_ops.h>

// ── Global instances ──────────────────────────────────────────────────────────

static Config        config;
static Display       display;
static Notifier      notifier;
static AircraftStore store;
static OtaUpdater    ota;
static WebUI         webUI(config, store, display, ota);
static ScreenMode    mode             = ScreenMode::Scanning;
static ScreenMode    preDebug         = ScreenMode::Scanning;
static int           debugScrollOffset = 0;

// Button state
static uint32_t lastInteractionTime = 0;
static uint32_t buttonPressTime     = 0;
static bool     longPressHandled    = false;
static uint32_t lastClickTime       = 0;
static constexpr uint32_t kIdleTimeoutMs       = 30000;
static constexpr uint32_t kLongPressDurationMs = 800;
static constexpr uint32_t kDoubleClickWindowMs = 400;

// ── WiFi ──────────────────────────────────────────────────────────────────────

static bool connectToWifi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.ssid, config.password);

    for (int frame = 0; frame < 80 && WiFi.status() != WL_CONNECTED; frame++)  {
        display.showSplash(frame);
        delay(250);
    }

    if (WiFi.status() == WL_CONNECTED) {
        webUI.setIPAddress(WiFi.localIP().toString());
        webUI.begin(mode, false);
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

    // Timeout - correct in memory and continue without rebooting
    config.pollIntervalMs = Config::kMinPollIntervalMs;
}

// ── Render ────────────────────────────────────────────────────────────────────

static void render() {
    display.setUpdateAvailable(ota.hasUpdate());
    switch (mode) {
        case ScreenMode::Scanning:
            if (store.hasActiveAircraft()) {
                const Aircraft* ac = store.currentAircraft();
                if (ac) {
                    int eta = ac->etaSeconds(config.latitude, config.longitude, config.radius);
                    display.showAircraft(*ac, false, 0, 0, eta);
                }
            } else if (store.consecutiveFailures() > 0) {
                display.showLostConnection(store.consecutiveFailures());
            } else {
                display.showScanning();
            }
            break;
        case ScreenMode::History:
            if (store.historyCount() > 0)
                display.showAircraft(store.historyAt(store.historyIndex()),
                                     true,
                                     store.historyIndex(),
                                     store.historyCount(),
                                     -1);
            else
                display.showScanning();
            break;
        case ScreenMode::Summary:
            display.showSummary(store.detectionCount(AircraftClass::Military),
                                store.detectionCount(AircraftClass::Medevac),
                                store.detectionCount(AircraftClass::Commercial),
                                store.detectionCount(AircraftClass::Private));
            break;
        case ScreenMode::Debug:
            display.showDebug(store.apiResponseLines(), debugScrollOffset,
                              store.lastResponseCode(), store.lastAircraftCount(),
                              webUI.ipAddress(), millis() / 1000,
                              OtaUpdater::currentVersion());
            break;
    }
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
    validatePollInterval();

    if (config.isUnconfigured() || !connectToWifi()) {
        webUI.begin(mode, true);   // AP mode: starts SoftAP + web server
        display.showSetupMode();
        return;  // loop() handles AP from here
    }

    display.showScanning();
    store.fetch(config, notifier, mode);
    render();
}

void loop() {
    // AP mode: serve web requests only
    if (webUI.isInSetupMode()) {
        webUI.processRequests();
        return;
    }

    M5.update();
    webUI.processRequests();

    // Web UI control button changed the screen mode - update the physical display immediately
    if (webUI.consumeControlChange()) {
        debugScrollOffset = 0;
        lastInteractionTime = millis();
        render();
    }

    // Idle timeout - return to Scanning after 30s of no interaction on History/Summary
    if ((mode == ScreenMode::History || mode == ScreenMode::Summary) &&
        millis() - lastInteractionTime >= kIdleTimeoutMs) {
        mode = ScreenMode::Scanning;
        render();
    }

    // Long-press detection
    if (M5.BtnA.wasPressed()) {
        buttonPressTime     = millis();
        longPressHandled    = false;
        lastInteractionTime = millis();
    }
    if (M5.BtnA.isPressed() && !longPressHandled &&
        (millis() - buttonPressTime >= kLongPressDurationMs)) {
        longPressHandled = true;
        if (mode == ScreenMode::Debug) {
            mode = preDebug;          // exit debug, restore previous screen
        } else {
            preDebug         = mode;  // remember where we came from
            debugScrollOffset = 0;
            mode             = ScreenMode::Debug;
        }
        render();
    }
    if (M5.BtnA.wasReleased() && !longPressHandled) {
        lastInteractionTime = millis();
        // Short press: scroll in debug, or cycle screens
        if (mode == ScreenMode::Debug) {
            uint32_t now = millis();
            if (now - lastClickTime <= kDoubleClickWindowMs) {
                lastClickTime = 0;  // reset so triple-click doesn't re-trigger
                notifier.sendTestNotification(config, display);
                render();
            } else {
                lastClickTime = now;
                int maxScroll = (int)store.apiResponseLines().size() - Display::kVisibleLineCount;
                if (maxScroll < 0) maxScroll = 0;
                debugScrollOffset = (debugScrollOffset >= maxScroll) ? 0 : debugScrollOffset + Display::kVisibleLineCount;
                render();
            }
        } else {
            switch (mode) {
                case ScreenMode::Scanning:
                    mode = (store.historyCount() > 0) ? ScreenMode::History : ScreenMode::Summary;
                    break;
                case ScreenMode::History:
                    if (store.historyIndex() < store.historyCount() - 1) {
                        store.setHistoryIndex(store.historyIndex() + 1);  // show next older entry
                    } else {
                        store.setHistoryIndex(0);  // reset for next visit
                        mode = ScreenMode::Summary;
                    }
                    break;
                case ScreenMode::Summary:  mode = ScreenMode::Scanning; break;
                default: break;
            }
            render();
        }
    }

    // Auto-cycle between multiple overhead aircraft every 5 seconds;
    // also redraw every second while live to keep ETA countdown fresh
    static uint32_t lastEtaRedraw = 0;
    if (mode == ScreenMode::Scanning && store.hasActiveAircraft()) {
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

    // Animate the scanning screen at ~20 fps when idle (skip if connection lost)
    static uint32_t lastScanRefresh = 0;
    if (mode == ScreenMode::Scanning && !store.hasActiveAircraft() &&
        store.consecutiveFailures() == 0 &&
        millis() - lastScanRefresh >= 50) {
        lastScanRefresh = millis();
        display.showScanning();
    }

    static uint32_t lastPoll = 0;
    if (millis() - lastPoll >= config.pollIntervalMs) {
        lastPoll = millis();
        store.fetch(config, notifier, mode);
        render();
    }

    // Daily OTA update check - notifies via ntfy if an update is found
    if (!webUI.isInSetupMode() && ota.isDue()) {
        if (ota.check()) {
            notifier.notifyUpdate(ota.latestVersion(), config);
        }
    }
}
