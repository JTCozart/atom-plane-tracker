#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <map>
#include <set>
#include <vector>
#include <cmath>
#include "secrets.h"

// adsb.lol — free, no API key, ADSBExchange v2 format, radius in NM
// Fallback: "https://api.adsb.one/v2/point/%.6f/%.6f/%.1f"
static const char* API_FMT = "https://api.adsb.lol/v2/lat/%.6f/lon/%.6f/dist/%.1f";

static const uint32_t POLL_MS = 30000;

// ── Types ─────────────────────────────────────────────────────────────────────

enum AcClass    { CLS_MIL = 0, CLS_MEDVAC, CLS_COMM, CLS_PRIV };
enum ScreenMode { SCR_SCAN, SCR_HIST, SCR_SUM, SCR_DEBUG };

struct Ac {
    String  icao;
    String  callsign;
    String  type;
    String  owner;
    float   alt;
    float   lat;
    float   lon;
    float   gs;     // ground speed, knots
    float   track;  // true track, degrees
    AcClass cls;
};

// ── State ─────────────────────────────────────────────────────────────────────

static ScreenMode           mode       = SCR_SCAN;
static ScreenMode           preDebug   = SCR_SCAN;   // mode to restore on debug exit
static std::map<String, Ac> inRadius;
static Ac                   lastAc;
static bool                 hasHist    = false;
static int                  cnt[4]     = {0, 0, 0, 0};

// Multi-aircraft cycling
static String               liveAcKey;
static uint32_t             acCycleTimer  = 0;
static const uint32_t       AC_CYCLE_MS   = 5000;

// Idle timeout — return to SCAN after 30s of no interaction on HIST/SUM
static uint32_t             lastInteractMs  = 0;
static const uint32_t       IDLE_TIMEOUT_MS = 30000;

// Debug state
static std::vector<String>  debugLines;
static int                  debugScroll  = 0;
static int                  lastHttpCode = 0;
static int                  lastAcTotal  = 0;

// Long-press detection
static uint32_t             btnDownAt      = 0;
static bool                 longPressFired = false;
static const uint32_t       LONG_PRESS_MS  = 800;

// ── Classification ────────────────────────────────────────────────────────────

// FAA LIFEGUARD prefix is a regulated designation — safe to match on callsign.
// All other classification comes from the API's own fields.
static const char* MEDVAC_CS[] = {
    "LIFEGRD", "MEDVAC", "AIRLIFE", nullptr
};
static const char* MEDVAC_OP[] = {
    "AIR LIFE", "AIR METHODS", "PHI AIR", "METRO AVIA",
    "OMNIFLIGHT", "GUARDIAN FL", nullptr
};

static bool startsWithAny(const String& s, const char** list) {
    for (; *list; list++) if (s.startsWith(*list)) return true;
    return false;
}
static bool containsAny(const String& s, const char** list) {
    for (; *list; list++) if (s.indexOf(*list) >= 0) return true;
    return false;
}

static AcClass classify(const String& callsign, const String& owner,
                         bool milFlag, const String& cat) {
    // Military: API flags only — no callsign guessing to avoid false positives
    if (milFlag) return CLS_MIL;

    String cs = callsign; cs.toUpperCase();
    String op = owner;    op.toUpperCase();

    // Medevac: FAA LIFEGUARD callsign prefix or known operator
    if (startsWithAny(cs, MEDVAC_CS)) return CLS_MEDVAC;
    if (containsAny(op, MEDVAC_OP))   return CLS_MEDVAC;

    // Commercial: API category A3=large, A4=high-vortex (B757), A5=heavy
    if (cat == "A3" || cat == "A4" || cat == "A5") return CLS_COMM;

    // Commercial: standard ICAO airline designator pattern (e.g. DAL123, UAL456)
    if (cs.length() >= 5 && cs.length() <= 8 &&
        isAlpha(cs[0]) && isAlpha(cs[1]) && isAlpha(cs[2]) && isDigit(cs[3]))
        return CLS_COMM;

    return CLS_PRIV;
}

// ── ETA calculation ───────────────────────────────────────────────────────────

// Returns seconds until the aircraft exits the query radius, or -1 if unknown.
// Steps the aircraft position forward 5-second increments along its track until
// it leaves the circle, capping at 60 minutes to avoid infinite loops.
static int etaSeconds(const Ac& ac) {
    if (ac.gs < 5.0f || ac.lat == 0.0f) return -1;

    const float R_NM      = QUERY_RADIUS_NM;
    const float DEG2RAD   = M_PI / 180.0f;
    const float NM_PER_DEG_LAT = 60.0f;

    float lat = ac.lat;
    float lon = ac.lon;
    float trackRad = ac.track * DEG2RAD;
    float speedNmPerSec = ac.gs / 3600.0f;

    for (int sec = 0; sec <= 3600; sec += 5) {
        float dLat = cosf(trackRad) * speedNmPerSec * 5.0f / NM_PER_DEG_LAT;
        float dLon = sinf(trackRad) * speedNmPerSec * 5.0f /
                     (NM_PER_DEG_LAT * cosf(lat * DEG2RAD));
        lat += dLat;
        lon += dLon;

        float dlat = (lat - QUERY_LAT) * NM_PER_DEG_LAT;
        float dlon = (lon - QUERY_LON) * NM_PER_DEG_LAT * cosf(QUERY_LAT * DEG2RAD);
        float dist = sqrtf(dlat * dlat + dlon * dlon);

        if (dist > R_NM) return sec;
    }
    return -1;  // still inside after 60 min
}

// ── Display ───────────────────────────────────────────────────────────────────

// Per-class: background, foreground, label
// GC9107 on Atom S3R swaps the green and blue channels, so GREEN renders
// as blue on screen and BLUE renders as green — constants are intentionally
// swapped here to produce the correct visible colors.
static const uint32_t BG[]    = { RED,   GREEN, BLUE,   YELLOW };
//  visible result on display:  { RED,   BLUE,  GREEN,  YELLOW }
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

    // Row 0 — class label
    M5.Display.setTextSize(1);
    M5.Display.setCursor(2, 2);
    M5.Display.print(LABEL[c]);

    // Row 1-2 — callsign, large
    M5.Display.setTextSize(2);
    M5.Display.setCursor(2, 14);
    String cs = ac.callsign.length() ? ac.callsign : "N/A";
    if (cs.length() > 10) cs = cs.substring(0, 10);
    M5.Display.print(cs);

    // Row 3 — type
    M5.Display.setTextSize(1);
    M5.Display.setCursor(2, 34);
    M5.Display.print("Type: ");
    M5.Display.print(ac.type.length() ? ac.type : "???");

    // Row 4 — altitude
    M5.Display.setCursor(2, 46);
    if (ac.alt > 0) {
        M5.Display.printf("Alt:  %.0f ft", ac.alt);
    } else {
        M5.Display.print("Alt:  ground");
    }

    // Row 5 — ETA until leaving radius (live only, not history)
    if (!hist) {
        M5.Display.setCursor(2, 58);
        int eta = etaSeconds(ac);
        if (eta < 0) {
            M5.Display.print("ETA:  --:--");
        } else {
            M5.Display.printf("ETA:  %d:%02d", eta / 60, eta % 60);
        }
    }

    // Row 6-7 — owner (up to two lines of 21 chars each)
    int ownerY = hist ? 58 : 70;
    M5.Display.setCursor(2, ownerY);
    M5.Display.print("Owner:");
    String owner = ac.owner.length() ? ac.owner : "Unknown";
    M5.Display.setCursor(2, ownerY + 12);
    if (owner.length() <= 21) {
        M5.Display.print(owner);
    } else {
        M5.Display.print(owner.substring(0, 21));
        M5.Display.setCursor(2, ownerY + 24);
        M5.Display.print(owner.substring(21, 42));
    }

    // History bar — red strip across the bottom
    if (hist) {
        M5.Display.fillRect(0, 116, 128, 12, RED);
        M5.Display.setTextColor(WHITE, RED);
        M5.Display.setCursor(30, 118);
        M5.Display.print("[ HISTORY ]");
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

static const int DBG_LINES_VISIBLE = 15;  // size-1 rows that fit below the header

static void drawDebug() {
    M5.Display.fillScreen(BLACK);
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.setTextSize(1);

    // Header row
    M5.Display.setCursor(0, 0);
    int total = (int)debugLines.size();
    int maxScroll = (total > DBG_LINES_VISIBLE) ? (total - DBG_LINES_VISIBLE) : 0;
    M5.Display.printf("DBG HTTP:%d ac:%d", lastHttpCode, lastAcTotal);

    // Content lines
    for (int i = 0; i < DBG_LINES_VISIBLE; i++) {
        int li = debugScroll + i;
        M5.Display.setCursor(0, 10 + i * 8);
        if (li < total) {
            // Truncate to 21 chars to stay on screen
            String line = debugLines[li];
            if (line.length() > 21) line = line.substring(0, 21);
            M5.Display.print(line);
        }
    }

    // Scroll indicator (bottom-right)
    if (maxScroll > 0) {
        M5.Display.setCursor(96, 120);
        M5.Display.printf("%d/%d", debugScroll, maxScroll);
    }
}

static void render() {
    switch (mode) {
        case SCR_SCAN:
            if (!inRadius.empty()) {
                auto it = inRadius.find(liveAcKey);
                if (it == inRadius.end()) it = inRadius.begin();
                drawAcScreen(it->second);
            } else {
                drawScan();
            }
            break;
        case SCR_HIST:  drawAcScreen(lastAc, /*hist=*/true); break;
        case SCR_SUM:   drawSummary();                        break;
        case SCR_DEBUG: drawDebug();                          break;
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

    int httpCode = http.GET();
    lastHttpCode = httpCode;
    if (httpCode != 200) { http.end(); return; }

    JsonDocument doc;
    if (deserializeJson(doc, http.getStream())) { http.end(); return; }
    http.end();

    // Rebuild debug lines from raw response
    JsonArray acArr = doc["ac"].as<JsonArray>();
    lastAcTotal = acArr.size();
    debugLines.clear();
    for (JsonObject plane : acArr) {
        String icao = plane["hex"]      | "??????";  icao.toUpperCase();
        String cs   = plane["flight"]   | "";        cs.trim();
        String reg  = plane["r"]        | "";
        String type = plane["t"]        | "??";
        String cat  = plane["category"] | "?";
        String own  = plane["ownOp"]    | "";
        float  alt  = plane["alt_baro"] | 0.0f;
        float  gs   = plane["gs"]       | 0.0f;
        bool   mil  = (plane["mil"] | 0) != 0 || ((plane["dbFlags"] | 0) & 1) != 0;

        char buf[48];
        snprintf(buf, sizeof(buf), "%s %s %s", icao.c_str(),
                 cs.length() ? cs.c_str() : reg.c_str(), type.c_str());
        debugLines.push_back(buf);

        snprintf(buf, sizeof(buf), " cat:%s mil:%c %5.0fft",
                 cat.c_str(), mil ? 'Y' : 'N', alt);
        debugLines.push_back(buf);

        if (own.length()) {
            debugLines.push_back(String(" ") + own);
        }
        debugLines.push_back("---");
    }
    if (debugLines.empty()) debugLines.push_back("No aircraft");

    // Don't reset scroll if user is actively viewing debug
    if (mode != SCR_DEBUG) debugScroll = 0;

    std::set<String> seen;

    for (JsonObject plane : acArr) {
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
        float  alt   = plane["alt_baro"] | 0.0f;
        float  lat   = plane["lat"]      | 0.0f;
        float  lon   = plane["lon"]      | 0.0f;
        float  gs    = plane["gs"]       | 0.0f;
        float  track = plane["track"]    | 0.0f;

        AcClass cls = classify(callsign, owner, milFlag, cat);

        Ac ac = { icao, callsign, type, owner, alt, lat, lon, gs, track, cls };
        inRadius[icao] = ac;
        cnt[cls]++;
        lastAc   = ac;
        hasHist  = true;

        // Interrupt to live view and pin to this new aircraft
        liveAcKey    = icao;
        acCycleTimer = millis();
        if (mode != SCR_DEBUG) mode = SCR_SCAN;
    }

    // Remove aircraft that no longer appear in the response
    for (auto it = inRadius.begin(); it != inRadius.end(); ) {
        if (!seen.count(it->first)) {
            it = inRadius.erase(it);
        } else {
            ++it;
        }
    }

    // If the aircraft we were showing left, snap to the next one
    if (!inRadius.empty() && !inRadius.count(liveAcKey)) {
        liveAcKey    = inRadius.begin()->first;
        acCycleTimer = millis();
    }
}

// ── Entry points ──────────────────────────────────────────────────────────────

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);

    connectWifi();
    drawScan();
    fetchAndUpdate();
    render();
}

void loop() {
    M5.update();

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
            preDebug     = mode;      // remember where we came from
            debugScroll  = 0;
            mode         = SCR_DEBUG;
        }
        render();
    }
    if (M5.BtnA.wasReleased() && !longPressFired) {
        lastInteractMs = millis();
        // Short press: scroll in debug, or cycle screens
        if (mode == SCR_DEBUG) {
            int maxScroll = (int)debugLines.size() - DBG_LINES_VISIBLE;
            if (maxScroll < 0) maxScroll = 0;
            debugScroll = (debugScroll >= maxScroll) ? 0 : debugScroll + DBG_LINES_VISIBLE;
        } else {
            switch (mode) {
                case SCR_SCAN: mode = hasHist ? SCR_HIST : SCR_SUM; break;
                case SCR_HIST: mode = SCR_SUM;                        break;
                case SCR_SUM:  mode = SCR_SCAN;                       break;
                default: break;
            }
        }
        render();
    }

    // Auto-cycle between multiple overhead aircraft every 5 seconds;
    // also redraw every second while live to keep ETA countdown fresh
    static uint32_t lastEtaRedraw = 0;
    if (mode == SCR_SCAN && !inRadius.empty()) {
        if (inRadius.size() > 1 && millis() - acCycleTimer >= AC_CYCLE_MS) {
            acCycleTimer = millis();
            auto it = inRadius.find(liveAcKey);
            if (it == inRadius.end() || ++it == inRadius.end())
                it = inRadius.begin();
            liveAcKey = it->first;
            render();
            lastEtaRedraw = millis();
        } else if (millis() - lastEtaRedraw >= 1000) {
            lastEtaRedraw = millis();
            render();
        }
    }

    static uint32_t lastPoll = 0;
    if (millis() - lastPoll >= POLL_MS) {
        lastPoll = millis();
        fetchAndUpdate();
        render();
    }
}
