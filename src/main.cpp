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

static Config        appCfg;
static Display       display;
static Notifier      notifier;
static AircraftStore store;
static WebUI         webUI(appCfg, store, display);
static ScreenMode    mode       = SCR_SCAN;
static ScreenMode    preDebug   = SCR_SCAN;
static int           debugScroll = 0;

// Button state
static uint32_t lastInteractMs = 0;
static uint32_t btnDownAt      = 0;
static bool     longPressFired = false;
static uint32_t lastClickMs    = 0;
static constexpr uint32_t IDLE_TIMEOUT_MS = 30000;
static constexpr uint32_t LONG_PRESS_MS   = 800;
static constexpr uint32_t DOUBLE_CLICK_MS = 400;

// ── WiFi ──────────────────────────────────────────────────────────────────────

static bool connectWifi() {
    M5.Display.fillScreen(M5.Display.color565(0, 0, 0));
    M5.Display.setTextColor(M5.Display.color565(255, 255, 255),
                            M5.Display.color565(0, 0, 0));
    M5.Display.setTextSize(1);
    M5.Display.setCursor(0, 0);
    M5.Display.println("Connecting WiFi...");
    M5.Display.println(appCfg.ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(appCfg.ssid, appCfg.pass);
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) delay(500);

    if (WiFi.status() == WL_CONNECTED) {
        webUI.setDeviceIP(WiFi.localIP().toString());
        webUI.begin(mode, false);
        M5.Display.println("Connected!");
        M5.Display.println(webUI.deviceIP().c_str());
        delay(800);
        return true;
    }
    M5.Display.println("Failed. Starting setup AP.");
    delay(1200);
    return false;
}

// ── Render ────────────────────────────────────────────────────────────────────

static void render() {
    switch (mode) {
        case SCR_SCAN:
            if (store.hasActive()) {
                const Ac* ac = store.displayAircraft();
                if (ac) {
                    int eta = ac->etaSeconds(appCfg.lat, appCfg.lon, appCfg.radius);
                    display.drawAircraft(*ac, false, 0, 0, eta);
                }
            } else {
                display.drawScan();
            }
            break;
        case SCR_HIST:
            if (store.historyCount() > 0)
                display.drawAircraft(store.historyAt(store.historyIndex()),
                                     true,
                                     store.historyIndex(),
                                     store.historyCount(),
                                     -1);
            else
                display.drawScan();
            break;
        case SCR_SUM:
            display.drawSummary(store.count(CLS_MIL),
                                store.count(CLS_MEDVAC),
                                store.count(CLS_COMM),
                                store.count(CLS_PRIV));
            break;
        case SCR_DEBUG:
            display.drawDebug(store.debugLines(), debugScroll,
                              store.lastHttpCode(), store.lastAcTotal(),
                              webUI.deviceIP());
            break;
    }
}

// ── Entry points ──────────────────────────────────────────────────────────────

void setup() {
    auto m5cfg = M5.config();
    M5.begin(m5cfg);

    display.begin();
    appCfg.load();

    if (!connectWifi()) {
        webUI.begin(mode, true);   // AP mode: starts SoftAP + web server
        display.drawAPMode();
        return;  // loop() handles AP from here
    }

    display.drawScan();
    store.fetch(appCfg, notifier, mode);
    render();
}

void loop() {
    // AP mode: serve web requests only
    if (webUI.isAPMode()) {
        webUI.handle();
        return;
    }

    M5.update();
    webUI.handle();

    // Idle timeout — return to SCAN after 30s of no interaction on HIST/SUM
    if ((mode == SCR_HIST || mode == SCR_SUM) &&
        millis() - lastInteractMs >= IDLE_TIMEOUT_MS) {
        mode = SCR_SCAN;
        render();
    }

    // Long-press detection
    if (M5.BtnA.wasPressed()) {
        btnDownAt      = millis();
        longPressFired = false;
        lastInteractMs = millis();
    }
    if (M5.BtnA.isPressed() && !longPressFired &&
        (millis() - btnDownAt >= LONG_PRESS_MS)) {
        longPressFired = true;
        if (mode == SCR_DEBUG) {
            mode = preDebug;          // exit debug, restore previous screen
        } else {
            preDebug    = mode;       // remember where we came from
            debugScroll = 0;
            mode        = SCR_DEBUG;
        }
        render();
    }
    if (M5.BtnA.wasReleased() && !longPressFired) {
        lastInteractMs = millis();
        // Short press: scroll in debug, or cycle screens
        if (mode == SCR_DEBUG) {
            uint32_t now = millis();
            if (now - lastClickMs <= DOUBLE_CLICK_MS) {
                lastClickMs = 0;  // reset so triple-click doesn't re-trigger
                notifier.sendTest(appCfg, display);
                render();
            } else {
                lastClickMs = now;
                int maxScroll = (int)store.debugLines().size() - Display::LINES_VISIBLE;
                if (maxScroll < 0) maxScroll = 0;
                debugScroll = (debugScroll >= maxScroll) ? 0 : debugScroll + Display::LINES_VISIBLE;
                render();
            }
        } else {
            switch (mode) {
                case SCR_SCAN:
                    mode = (store.historyCount() > 0) ? SCR_HIST : SCR_SUM;
                    break;
                case SCR_HIST:
                    if (store.historyIndex() < store.historyCount() - 1) {
                        store.setHistoryIndex(store.historyIndex() + 1);  // show next older entry
                    } else {
                        store.setHistoryIndex(0);  // reset for next visit
                        mode = SCR_SUM;
                    }
                    break;
                case SCR_SUM:  mode = SCR_SCAN; break;
                default: break;
            }
            render();
        }
    }

    // Auto-cycle between multiple overhead aircraft every 5 seconds;
    // also redraw every second while live to keep ETA countdown fresh
    static uint32_t lastEtaRedraw = 0;
    if (mode == SCR_SCAN && store.hasActive()) {
        if (store.activeCount() > 1 &&
            millis() - store.cycleTimer() >= AircraftStore::CYCLE_MS) {
            store.advanceCycle();
            render();
            lastEtaRedraw = millis();
        } else if (millis() - lastEtaRedraw >= 1000) {
            lastEtaRedraw = millis();
            render();
        }
    }

    static uint32_t lastPoll = 0;
    if (millis() - lastPoll >= appCfg.pollMs) {
        lastPoll = millis();
        store.fetch(appCfg, notifier, mode);
        render();
    }
}
