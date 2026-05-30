#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "AppState.h"
#include "Aircraft.h"
#include "Config.h"
#include "Display.h"
#include "Notifier.h"
#include "AircraftStore.h"
#include "WebUI.h"
#include "secrets.h"

// ── Global instances ──────────────────────────────────────────────────────────

static Config        config;
static Display       display;
static Notifier      notifier;
static AircraftStore store;
static WebUI         webUI(config, store, display);
static ScreenMode    mode             = ScreenMode::Scanning;
static ScreenMode    preDebug         = ScreenMode::Scanning;
static int           debugScrollOffset = 0;

// Button state
static uint32_t lastInteractionTime = 0;
static uint32_t buttonPressTime     = 0;
static bool     longPressHandled    = false;
static uint32_t lastClickTime       = 0;
static int      clickCount          = 0;
static bool     displayOn           = true;
static constexpr uint32_t kIdleTimeoutMs       = 30000;
static constexpr uint32_t kLongPressDurationMs = 800;
static constexpr uint32_t kDoubleClickWindowMs = 400;
static constexpr uint32_t kTripleClickWindowMs = 600;

// ── WiFi ──────────────────────────────────────────────────────────────────────

static bool connectToWifi() {
    M5.Display.fillScreen(M5.Display.color565(0, 0, 0));
    M5.Display.setTextColor(M5.Display.color565(255, 255, 255),
                            M5.Display.color565(0, 0, 0));
    M5.Display.setTextSize(1);
    M5.Display.setCursor(0, 0);
    M5.Display.println("Connecting WiFi...");
    M5.Display.println(config.ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(config.ssid, config.password);
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) delay(500);

    if (WiFi.status() == WL_CONNECTED) {
        webUI.setIPAddress(WiFi.localIP().toString());
        webUI.begin(mode, false);
        M5.Display.println("Connected!");
        M5.Display.println(webUI.ipAddress().c_str());
        delay(800);
        return true;
    }
    M5.Display.println("Failed. Starting setup AP.");
    delay(1200);
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

    // Timeout — correct in memory and continue without rebooting
    config.pollIntervalMs = Config::kMinPollIntervalMs;
}

// ── Render ────────────────────────────────────────────────────────────────────

static void render() {
    if (!displayOn) {
        M5.Display.fillScreen(M5.Display.color565(0, 0, 0));
        return;
    }

    switch (mode) {
        case ScreenMode::Scanning:
            if (store.hasActiveAircraft()) {
                const Aircraft* ac = store.currentAircraft();
                if (ac) {
                    int eta = ac->etaSeconds(config.latitude, config.longitude, config.radius);
                    display.showAircraft(*ac, false, 0, 0, eta);
                }
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
                              webUI.ipAddress());
            break;
    }
}

// ── Entry points ──────────────────────────────────────────────────────────────

void setup() {
    auto m5cfg = M5.config();
    M5.begin(m5cfg);

    display.begin();
    config.load();
    validatePollInterval();

    if (!connectToWifi()) {
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

    // Idle timeout — return to Scanning after 30s of no interaction on History/Summary
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
        uint32_t now = millis();

        // Triple-click detection: turn display on/off (works from any mode)
        if (clickCount == 0) {
            clickCount = 1;
            lastClickTime = now;
        } else if (now - lastClickTime <= kTripleClickWindowMs) {
            clickCount++;
            if (clickCount == 3) {
                displayOn = !displayOn;
                clickCount = 0;
                render();
                return;
            }
        } else {
            // Time window expired, reset and start counting again
            clickCount = 1;
            lastClickTime = now;
        }

        // Short press: scroll in debug, or cycle screens (unless we're tracking clicks for triple-click)
        if (clickCount > 1) return;  // Still within triple-click detection window

        if (mode == ScreenMode::Debug) {
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

    // Animate the scanning screen at ~20 fps when idle
    static uint32_t lastScanRefresh = 0;
    if (mode == ScreenMode::Scanning && !store.hasActiveAircraft() &&
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
}
