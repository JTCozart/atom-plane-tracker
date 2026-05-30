#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <map>
#include <set>
#include "secrets.h"

// airplanes.live — free, no API key, ADSBExchange v2 format, radius in NM
// Fallback: "https://api.adsb.lol/v2/lat/%.6f/lon/%.6f/dist/%.1f"
static const char* API_FMT = "https://api.adsb.one/v2/point/%.6f/%.6f/%.1f";

static const uint32_t POLL_MS    = 10000;
static const uint16_t BEEP_HZ    = 2000;
static const uint16_t BEEP_ON_MS = 120;
static const uint16_t BEEP_GAP_MS= 150;

// ── Types ─────────────────────────────────────────────────────────────────────

enum AcClass    { CLS_MIL = 0, CLS_MEDVAC, CLS_COMM, CLS_PRIV };
enum ScreenMode { SCR_SCAN, SCR_HIST, SCR_SUM };

struct Ac {
    String  icao;
    String  callsign;
    String  type;
    String  owner;
    AcClass cls;
};

// ── State ─────────────────────────────────────────────────────────────────────

static ScreenMode           mode    = SCR_SCAN;
static std::map<String, Ac> inRadius;       // ICAO → aircraft currently overhead
static Ac                   lastAc;
static bool                 hasHist = false;
static int                  cnt[4]  = {0, 0, 0, 0};  // indexed by AcClass

// ── Classification ────────────────────────────────────────────────────────────

static const char* MIL_PFX[] = {
    "AFA","RCH","NAF","CMF","AAM","CGD","PAT","DUKE",
    "HUNT","JAKE","VALOR","SAM","VENUS","EVAC", nullptr
};
static const char* MEDVAC_CS[] = {
    "LIFEGRD","MEDVAC","AIRLIFE","AIRLFE","LIFEFL", nullptr
};
static const char* MEDVAC_OP[] = {
    "AIR LIFE","AIR METHODS","MEDEVAC","PHI AIR","METRO AVIA",
    "OMNIFLIGHT","GUARDIAN FL", nullptr
};

static bool startsWith(const String& s, const char** list) {
    for (; *list; list++) if (s.startsWith(*list)) return true;
    return false;
}
static bool contains(const String& s, const char** list) {
    for (; *list; list++) if (s.indexOf(*list) >= 0) return true;
    return false;
}

static AcClass classify(const String& callsign, const String& owner,
                         bool milFlag, const String& cat) {
    if (milFlag) return CLS_MIL;

    String cs = callsign; cs.toUpperCase();
    String op = owner;    op.toUpperCase();

    if (startsWith(cs, MIL_PFX))  return CLS_MIL;
    if (startsWith(cs, MEDVAC_CS)) return CLS_MEDVAC;
    if (contains(op, MEDVAC_OP))   return CLS_MEDVAC;

    // Large aircraft categories (A3=large, A4=high-vortex, A5=heavy)
    if (cat == "A3" || cat == "A4" || cat == "A5") return CLS_COMM;

    // ICAO airline code pattern: 3 alpha + digits (e.g. UAL123, DAL456)
    if (cs.length() >= 5 &&
        isAlpha(cs[0]) && isAlpha(cs[1]) && isAlpha(cs[2]) && isDigit(cs[3]))
        return CLS_COMM;

    return CLS_PRIV;
}

// ── Beep ──────────────────────────────────────────────────────────────────────

static const int BEEP_CNT[] = {5, 3, 2, 1};  // indexed by AcClass

static void doBeep(AcClass cls) {
    int n = BEEP_CNT[cls];
    for (int i = 0; i < n; i++) {
        M5.Speaker.tone(BEEP_HZ, BEEP_ON_MS);
        delay(BEEP_ON_MS + BEEP_GAP_MS);
    }
    M5.Speaker.stop();
}

// ── Display ───────────────────────────────────────────────────────────────────

// Per-class: background, foreground, label
static const uint32_t BG[]    = { RED,   BLUE,  GREEN,  YELLOW };
static const uint32_t FG[]    = { BLACK, WHITE, BLACK,  BLACK  };
static const char*    LABEL[] = { "MILITARY", "MEDEVAC", "COMMERCIAL", "PRIVATE" };

static void drawScan() {
    M5.Display.fillScreen(BLACK);
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.setTextSize(2);
    // Center "SCANNING" — each char 12px wide, 8 chars = 96px; (128-96)/2 = 16
    M5.Display.setCursor(16, 56);
    M5.Display.print("SCANNING");
}

static void drawAcScreen(const Ac& ac, bool hist = false) {
    AcClass c = ac.cls;
    M5.Display.fillScreen(BG[c]);
    M5.Display.setTextColor(FG[c], BG[c]);

    // Row 0 — class label + optional [HIST] tag
    M5.Display.setTextSize(1);
    M5.Display.setCursor(2, 2);
    M5.Display.print(LABEL[c]);
    if (hist) {
        M5.Display.setCursor(84, 2);
        M5.Display.print("[HIST]");
    }

    // Row 1-2 — callsign, large
    M5.Display.setTextSize(2);
    M5.Display.setCursor(2, 14);
    String cs = ac.callsign.length() ? ac.callsign : "N/A";
    if (cs.length() > 10) cs = cs.substring(0, 10);
    M5.Display.print(cs);

    // Row 3 — aircraft type
    M5.Display.setTextSize(1);
    M5.Display.setCursor(2, 48);
    M5.Display.print("Type: ");
    M5.Display.print(ac.type.length() ? ac.type : "???");

    // Row 4-6 — owner (up to two lines of 21 chars each)
    M5.Display.setCursor(2, 62);
    M5.Display.print("Owner:");
    String owner = ac.owner.length() ? ac.owner : "Unknown";
    M5.Display.setCursor(2, 74);
    if (owner.length() <= 21) {
        M5.Display.print(owner);
    } else {
        M5.Display.print(owner.substring(0, 21));
        M5.Display.setCursor(2, 86);
        M5.Display.print(owner.substring(21, 42));
    }
}

static void drawSummary() {
    M5.Display.fillScreen(BLACK);
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(8, 4);
    M5.Display.print("SUMMARY");

    M5.Display.setTextSize(1);
    int y = 38;
    M5.Display.setCursor(4, y); M5.Display.printf("Military:   %d", cnt[CLS_MIL]);    y += 14;
    M5.Display.setCursor(4, y); M5.Display.printf("Medevac:    %d", cnt[CLS_MEDVAC]); y += 14;
    M5.Display.setCursor(4, y); M5.Display.printf("Commercial: %d", cnt[CLS_COMM]);   y += 14;
    M5.Display.setCursor(4, y); M5.Display.printf("Private:    %d", cnt[CLS_PRIV]);
}

static void render() {
    if (!inRadius.empty()) {
        // Always show highest-priority live aircraft (lowest enum value wins)
        Ac* best = nullptr;
        for (auto& [id, a] : inRadius)
            if (!best || a.cls < best->cls) best = &a;
        drawAcScreen(*best);
    } else {
        switch (mode) {
            case SCR_SCAN: drawScan();                           break;
            case SCR_HIST: drawAcScreen(lastAc, /*hist=*/true); break;
            case SCR_SUM:  drawSummary();                        break;
        }
    }
}

// ── WiFi ──────────────────────────────────────────────────────────────────────

static void connectWifi() {
    M5.Display.fillScreen(BLACK);
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(0, 0);
    M5.Display.println("Connecting WiFi...");
    M5.Display.println(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) delay(500);

    if (WiFi.status() == WL_CONNECTED) {
        M5.Display.println("Connected!");
    } else {
        M5.Display.println("FAILED - retrying");
    }
    delay(800);
}

// ── Fetch & state update ──────────────────────────────────────────────────────

static void fetchAndUpdate() {
    if (WiFi.status() != WL_CONNECTED) { connectWifi(); return; }

    char url[128];
    snprintf(url, sizeof(url), API_FMT,
             (double)QUERY_LAT, (double)QUERY_LON, (double)QUERY_RADIUS_NM);

    WiFiClientSecure client;
    client.setInsecure();  // skip cert validation on embedded
    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(8000);

    if (http.GET() != 200) { http.end(); return; }

    JsonDocument doc;
    if (deserializeJson(doc, http.getStream())) { http.end(); return; }
    http.end();

    std::set<String> seen;

    for (JsonObject plane : doc["ac"].as<JsonArray>()) {
        const char* hex = plane["hex"] | "";
        if (!hex || hex[0] == '\0') continue;

        String icao = hex;
        icao.toUpperCase();
        seen.insert(icao);

        if (inRadius.count(icao)) continue;  // already tracking this aircraft

        bool milFlag = (plane["mil"] | 0) != 0 || ((plane["dbFlags"] | 0) & 1) != 0;

        const char* cs_raw = plane["flight"] | "";
        String callsign = cs_raw;
        callsign.trim();

        String type  = plane["t"]        | "";
        String owner = plane["ownOp"]    | "";
        String cat   = plane["category"] | "";

        AcClass cls = classify(callsign, owner, milFlag, cat);

        Ac ac = { icao, callsign, type, owner, cls };
        inRadius[icao] = ac;
        cnt[cls]++;
        lastAc  = ac;
        hasHist = true;

        // Beep before render so the alert is immediate on detection
        doBeep(cls);
    }

    // Remove aircraft that no longer appear in the response
    bool anyLeft = false;
    for (auto it = inRadius.begin(); it != inRadius.end(); ) {
        if (!seen.count(it->first)) {
            it = inRadius.erase(it);
        } else {
            ++it;
            anyLeft = true;
        }
    }

    if (!anyLeft && inRadius.empty()) {
        mode = SCR_SCAN;  // all aircraft gone, return to scanning
    }
}

// ── Entry points ──────────────────────────────────────────────────────────────

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Speaker.setVolume(180);

    connectWifi();
    drawScan();
}

void loop() {
    M5.update();

    // Button cycles view — only when no aircraft is actively overhead
    if (M5.BtnA.wasPressed() && inRadius.empty()) {
        switch (mode) {
            case SCR_SCAN: mode = hasHist ? SCR_HIST : SCR_SUM;  break;
            case SCR_HIST: mode = SCR_SUM;                         break;
            case SCR_SUM:  mode = SCR_SCAN;                        break;
        }
        render();
    }

    static uint32_t lastPoll = 0;
    if (millis() - lastPoll >= POLL_MS) {
        lastPoll = millis();
        fetchAndUpdate();
        render();
    }
}
