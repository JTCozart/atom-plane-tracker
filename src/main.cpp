#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <map>
#include <set>
#include <vector>
#include <cmath>
#include "secrets.h"

// ── Runtime config (loaded from NVS, falls back to secrets.h) ────────────────

struct Config {
    char     ssid[64];
    char     pass[64];
    double   lat;
    double   lon;
    float    radius;
    uint32_t pollMs;
};
static Config appCfg;

static void loadConfig() {
    Preferences prefs;
    prefs.begin("plantracker", true);
    String storedSsid = prefs.getString("ssid", "");
    if (storedSsid.length() > 0) {
        strncpy(appCfg.ssid, storedSsid.c_str(),                          sizeof(appCfg.ssid) - 1);
        strncpy(appCfg.pass, prefs.getString("pass", WIFI_PASSWORD).c_str(), sizeof(appCfg.pass) - 1);
        appCfg.lat    = prefs.getDouble("lat",    QUERY_LAT);
        appCfg.lon    = prefs.getDouble("lon",    QUERY_LON);
        appCfg.radius = prefs.getFloat ("radius", QUERY_RADIUS_NM);
        appCfg.pollMs = prefs.getUInt  ("pollMs", POLL_INTERVAL_MS);
    } else {
        strncpy(appCfg.ssid, WIFI_SSID,      sizeof(appCfg.ssid) - 1);
        strncpy(appCfg.pass, WIFI_PASSWORD,  sizeof(appCfg.pass) - 1);
        appCfg.lat    = QUERY_LAT;
        appCfg.lon    = QUERY_LON;
        appCfg.radius = QUERY_RADIUS_NM;
        appCfg.pollMs = POLL_INTERVAL_MS;
    }
    prefs.end();
}

// adsb.lol — free, no API key, ADSBExchange v2 format, radius in NM
// Fallback: "https://api.adsb.one/v2/point/%.6f/%.6f/%.1f"
static const char* API_FMT = "https://api.adsb.lol/v2/lat/%.6f/lon/%.6f/dist/%.1f";

// Poll interval defined in secrets.h as POLL_INTERVAL_MS

// ── Types ─────────────────────────────────────────────────────────────────────

enum AcClass    { CLS_MIL = 0, CLS_MEDVAC, CLS_COMM, CLS_PRIV };
enum ScreenMode { SCR_SCAN, SCR_HIST, SCR_SUM, SCR_DEBUG };

struct Ac {
    String   icao;
    String   callsign;
    String   type;
    String   owner;
    float    alt;
    float    lat;
    float    lon;
    float    gs;          // ground speed, knots
    float    track;       // true track, degrees
    uint32_t lastFixMs;   // millis() when lat/lon/gs/track were last updated
    AcClass  cls;
};

// ── State ─────────────────────────────────────────────────────────────────────

static ScreenMode           mode       = SCR_SCAN;
static ScreenMode           preDebug   = SCR_SCAN;
static std::map<String, Ac> inRadius;
static int                  cnt[4]     = {0, 0, 0, 0};

// History log — newest at index 0, up to 5 entries
static Ac  histLog[5];
static int histCount = 0;  // entries stored (0-5)
static int histIdx   = 0;  // which entry is on screen

static void addToHistory(const Ac& ac) {
    int slots = (histCount < 5) ? histCount : 4;
    for (int i = slots; i > 0; i--) histLog[i] = histLog[i - 1];
    histLog[0] = ac;
    if (histCount < 5) histCount++;
    histIdx = 0;  // always show newest on next HIST visit
}

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

// Forward declarations for web server handlers (defined after display section)
static void handleRoot();
static void handleSave();

// Device IP — set after WiFi connect or AP start; shown on debug screen
static String deviceIP = "0.0.0.0";

// Long-press and double-click detection
static uint32_t             btnDownAt        = 0;
static bool                 longPressFired   = false;
static const uint32_t       LONG_PRESS_MS    = 800;
static uint32_t             lastClickMs      = 0;
static const uint32_t       DOUBLE_CLICK_MS  = 400;

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

    const float R_NM           = appCfg.radius;
    const float DEG2RAD        = M_PI / 180.0f;
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

        float dlat = (lat - (float)appCfg.lat) * NM_PER_DEG_LAT;
        float dlon = (lon - (float)appCfg.lon) * NM_PER_DEG_LAT * cosf((float)appCfg.lat * DEG2RAD);
        float dist = sqrtf(dlat * dlat + dlon * dlon);

        if (dist > R_NM) return sec;
    }
    return -1;  // still inside after 60 min
}

// ── Display ───────────────────────────────────────────────────────────────────

// All colors are uint16_t RGB565 values produced by color565() at runtime.
// Using named constants (RED, GREEN, etc.) was causing wrong colors because
// those macros are 16-bit RGB565 values misread as 24-bit RGB888 by LGFX.
static uint16_t C_BLACK, C_WHITE, C_RED;
static uint16_t BG[4];   // per AcClass background
static uint16_t FG[4];   // per AcClass foreground text
static const char* LABEL[] = { "MILITARY", "MEDEVAC", "COMMERCIAL", "PRIVATE" };

static void initColors() {
    C_BLACK = M5.Display.color565(0,   0,   0);
    C_WHITE = M5.Display.color565(255, 255, 255);
    C_RED   = M5.Display.color565(255, 0,   0);

    BG[CLS_MIL]    = M5.Display.color565(255, 0,   0);    // red
    BG[CLS_MEDVAC] = M5.Display.color565(0,   0,   255);  // blue
    BG[CLS_COMM]   = M5.Display.color565(0,   255, 0);    // green
    BG[CLS_PRIV]   = M5.Display.color565(255, 255, 0);    // yellow

    FG[CLS_MIL]    = C_BLACK;
    FG[CLS_MEDVAC] = C_WHITE;
    FG[CLS_COMM]   = C_BLACK;
    FG[CLS_PRIV]   = C_BLACK;
}

static void drawScan() {
    M5.Display.fillScreen(C_BLACK);
    M5.Display.setTextColor(C_WHITE, C_BLACK);
    M5.Display.setTextSize(2);
    // Center "SCANNING" — each char 12px wide, 8 chars = 96px; (128-96)/2 = 16
    M5.Display.setCursor(16, 56);
    M5.Display.print("SCANNING");
}

static void drawAPMode() {
    M5.Display.fillScreen(C_BLACK);
    M5.Display.setTextColor(C_WHITE, C_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(14, 4);
    M5.Display.print("SETUP");

    M5.Display.setTextSize(1);
    M5.Display.setCursor(2, 30); M5.Display.print("SSID: PlaneTracker");
    M5.Display.setCursor(2, 42); M5.Display.print("Pass: PlaneTracker");
    M5.Display.setCursor(2, 60); M5.Display.print("Open browser:");
    M5.Display.setCursor(2, 72); M5.Display.print("192.168.4.1");
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
        int rawEta = etaSeconds(ac);
        if (rawEta < 0) {
            M5.Display.print("ETA:  --:--");
        } else {
            int elapsed = (int)((millis() - ac.lastFixMs) / 1000);
            int eta = max(0, rawEta - elapsed);
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

    // History bar — red strip across the bottom with entry counter
    if (hist) {
        M5.Display.fillRect(0, 116, 128, 12, C_RED);
        M5.Display.setTextColor(C_WHITE, C_RED);
        char bar[24];
        snprintf(bar, sizeof(bar), "[ HIST %d/%d ]", histIdx + 1, histCount);
        int x = max(0, (128 - (int)strlen(bar) * 6) / 2);
        M5.Display.setCursor(x, 118);
        M5.Display.print(bar);
    }
}

static void drawSummary() {
    M5.Display.fillScreen(C_BLACK);
    M5.Display.setTextColor(C_WHITE, C_BLACK);
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

static const int DBG_LINES_VISIBLE = 13;  // size-1 rows below the two-line header
static const int DBG_CONTENT_Y     = 20;  // y offset where content starts

static void drawDebug() {
    M5.Display.fillScreen(C_BLACK);
    M5.Display.setTextColor(C_WHITE, C_BLACK);
    M5.Display.setTextSize(1);

    // Header row 1 — HTTP status and aircraft count
    M5.Display.setCursor(0, 0);
    M5.Display.printf("DBG HTTP:%d ac:%d", lastHttpCode, lastAcTotal);

    // Header row 2 — device IP (tap-able via browser for config)
    M5.Display.setCursor(0, 10);
    M5.Display.printf("IP: %s", deviceIP.c_str());

    // Content lines
    int total     = (int)debugLines.size();
    int maxScroll = max(0, total - DBG_LINES_VISIBLE);
    for (int i = 0; i < DBG_LINES_VISIBLE; i++) {
        int li = debugScroll + i;
        M5.Display.setCursor(0, DBG_CONTENT_Y + i * 8);
        if (li < total) {
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
        case SCR_HIST:
            if (histCount > 0) drawAcScreen(histLog[histIdx], true);
            else               drawScan();
            break;
        case SCR_SUM:   drawSummary();                        break;
        case SCR_DEBUG: drawDebug();                          break;
    }
}

// ── ntfy notifications ────────────────────────────────────────────────────────

static const char* classTag(AcClass cls) {
    switch (cls) {
        case CLS_MIL:    return "MIL";
        case CLS_MEDVAC: return "MEDVAC";
        case CLS_COMM:   return "COMM";
        default:         return "PRIV";
    }
}

static void sendNtfy(const Ac& ac) {
    if (strlen(NTFY_TOKEN) == 0 || strlen(NTFY_TOPIC) == 0) return;

    // If NTFY_CLASSES is set, only notify for listed classes
    if (strlen(NTFY_CLASSES) > 0 && strstr(NTFY_CLASSES, classTag(ac.cls)) == nullptr) return;

    const char* priority;
    const char* tags;
    switch (ac.cls) {
        case CLS_MIL:    priority = "urgent";  tags = "rotating_light"; break;
        case CLS_MEDVAC: priority = "high";    tags = "ambulance";      break;
        case CLS_COMM:   priority = "default"; tags = "airplane";       break;
        default:         priority = "low";     tags = "small_airplane"; break;
    }

    String cs = ac.callsign.length() ? ac.callsign : (ac.type.length() ? ac.type : "Unknown");
    char body[160];
    snprintf(body, sizeof(body), "%s (%s) at %.0f ft\nOwner: %s",
             cs.c_str(),
             ac.type.length() ? ac.type.c_str() : "???",
             ac.alt,
             ac.owner.length() ? ac.owner.c_str() : "Unknown");

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, String("https://ntfy.sh/") + NTFY_TOPIC);
    http.addHeader("Authorization", String("Bearer ") + NTFY_TOKEN);
    http.addHeader("Content-Type",  "text/plain");
    http.addHeader("Title",    String(LABEL[ac.cls]) + " Aircraft Detected");
    http.addHeader("Priority", priority);
    http.addHeader("Tags",     tags);
    http.POST(body);
    http.end();
}

static void sendTestNtfy() {
    if (strlen(NTFY_TOKEN) == 0 || strlen(NTFY_TOPIC) == 0) {
        M5.Display.fillScreen(C_BLACK);
        M5.Display.setTextColor(C_WHITE, C_BLACK);
        M5.Display.setTextSize(1);
        M5.Display.setCursor(2, 56);
        M5.Display.print("ntfy not configured");
        delay(1500);
        return;
    }

    M5.Display.fillScreen(C_BLACK);
    M5.Display.setTextColor(C_WHITE, C_BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(2, 56);
    M5.Display.print("Sending test...");

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, String("https://ntfy.sh/") + NTFY_TOPIC);
    http.addHeader("Authorization", String("Bearer ") + NTFY_TOKEN);
    http.addHeader("Content-Type",  "text/plain");
    http.addHeader("Title",    "atom-plane-tracker test");
    http.addHeader("Priority", "default");
    http.addHeader("Tags",     "white_check_mark");
    int code = http.POST("Test notification from atom-plane-tracker");
    http.end();

    M5.Display.setCursor(2, 70);
    M5.Display.printf("HTTP %d", code);
    delay(1500);
}

// ── AP / config web server ────────────────────────────────────────────────────

static WebServer apServer(80);
static bool      inAPMode  = false;

static void startWebServer() {
    apServer.on("/",     HTTP_GET,  handleRoot);
    apServer.on("/save", HTTP_POST, handleSave);
    apServer.begin();
}

static void handleRoot() {
    String html =
        F("<!DOCTYPE html><html><head><title>PlaneTracker Setup</title>"
          "<meta name='viewport' content='width=device-width,initial-scale=1'>"
          "<style>"
          "body{font-family:sans-serif;max-width:420px;margin:24px auto;padding:0 12px}"
          "h2{margin-bottom:16px}label{display:block;margin-top:10px;font-size:.9em}"
          "input{width:100%;padding:7px;box-sizing:border-box;margin-top:3px;font-size:1em}"
          "button{margin-top:18px;width:100%;padding:10px;background:#1976D2;"
          "color:#fff;border:none;font-size:1em;cursor:pointer;border-radius:4px}"
          "</style></head><body><h2>&#9992; PlaneTracker Setup</h2>"
          "<form method='POST' action='/save'>");

    html += "<label>WiFi SSID</label>"
            "<input name='ssid' value='" + String(appCfg.ssid) + "'>";
    html += "<label>WiFi Password</label>"
            "<input name='pass' type='password' placeholder='leave blank to keep current'>";
    html += "<label>Latitude</label>"
            "<input name='lat' value='" + String(appCfg.lat, 6) + "'>";
    html += "<label>Longitude</label>"
            "<input name='lon' value='" + String(appCfg.lon, 6) + "'>";
    html += "<label>Search Radius (nautical miles)</label>"
            "<input name='radius' value='" + String(appCfg.radius) + "'>";
    html += "<label>Poll Interval (ms)</label>"
            "<input name='poll' value='" + String(appCfg.pollMs) + "'>";
    html += "<button type='submit'>Save &amp; Reboot</button>"
            "</form></body></html>";

    apServer.send(200, "text/html", html);
}

static void handleSave() {
    Preferences prefs;
    prefs.begin("plantracker", false);

    if (apServer.hasArg("ssid") && apServer.arg("ssid").length() > 0)
        prefs.putString("ssid", apServer.arg("ssid"));
    if (apServer.hasArg("pass") && apServer.arg("pass").length() > 0)
        prefs.putString("pass", apServer.arg("pass"));
    if (apServer.hasArg("lat"))
        prefs.putDouble("lat",    apServer.arg("lat").toDouble());
    if (apServer.hasArg("lon"))
        prefs.putDouble("lon",    apServer.arg("lon").toDouble());
    if (apServer.hasArg("radius"))
        prefs.putFloat ("radius", apServer.arg("radius").toFloat());
    if (apServer.hasArg("poll"))
        prefs.putUInt  ("pollMs", (uint32_t)apServer.arg("poll").toInt());

    prefs.end();

    apServer.send(200, "text/html",
        F("<!DOCTYPE html><html><body style='font-family:sans-serif;text-align:center;margin-top:60px'>"
          "<h2>Saved! Rebooting&hellip;</h2></body></html>"));
    delay(1500);
    ESP.restart();
}

static void startAPMode() {
    inAPMode  = true;
    deviceIP  = "192.168.4.1";
    WiFi.mode(WIFI_AP);
    WiFi.softAP("PlaneTracker", "PlaneTracker");
    startWebServer();
    drawAPMode();
}

// ── WiFi ──────────────────────────────────────────────────────────────────────

static bool connectWifi() {
    M5.Display.fillScreen(C_BLACK);
    M5.Display.setTextColor(C_WHITE, C_BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(0, 0);
    M5.Display.println("Connecting WiFi...");
    M5.Display.println(appCfg.ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(appCfg.ssid, appCfg.pass);
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) delay(500);

    if (WiFi.status() == WL_CONNECTED) {
        deviceIP = WiFi.localIP().toString();
        startWebServer();
        M5.Display.println("Connected!");
        M5.Display.println(deviceIP.c_str());
        delay(800);
        return true;
    }
    M5.Display.println("Failed. Starting setup AP.");
    delay(1200);
    return false;
}

// ── Fetch & state update ──────────────────────────────────────────────────────

static void fetchAndUpdate() {
    if (WiFi.status() != WL_CONNECTED) {
        if (!connectWifi()) { startAPMode(); return; }
    }

    char url[128];
    snprintf(url, sizeof(url), API_FMT,
             appCfg.lat, appCfg.lon, (double)appCfg.radius);

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

        float  alt   = plane["alt_baro"] | 0.0f;
        float  lat   = plane["lat"]      | 0.0f;
        float  lon   = plane["lon"]      | 0.0f;
        float  gs    = plane["gs"]       | 0.0f;
        float  track = plane["track"]    | 0.0f;

        if (inRadius.count(icao)) {
            // Refresh position/speed data so ETA stays accurate
            Ac& existing = inRadius[icao];
            existing.alt       = alt;
            existing.lat       = lat;
            existing.lon       = lon;
            existing.gs        = gs;
            existing.track     = track;
            existing.lastFixMs = millis();
            continue;
        }

        bool milFlag = (plane["mil"] | 0) != 0 || ((plane["dbFlags"] | 0) & 1) != 0;

        const char* cs_raw = plane["flight"] | "";
        String callsign = cs_raw;
        callsign.trim();

        String type  = plane["t"]        | "";
        String owner = plane["ownOp"]    | "";
        String cat   = plane["category"] | "";

        AcClass cls = classify(callsign, owner, milFlag, cat);

        Ac ac = { icao, callsign, type, owner, alt, lat, lon, gs, track, millis(), cls };
        inRadius[icao] = ac;
        cnt[cls]++;
        addToHistory(ac);

        // Interrupt to live view and pin to this new aircraft
        liveAcKey    = icao;
        acCycleTimer = millis();
        if (mode != SCR_DEBUG) mode = SCR_SCAN;

        sendNtfy(ac);
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
    auto m5cfg = M5.config();
    M5.begin(m5cfg);

    initColors();
    loadConfig();

    if (!connectWifi()) {
        startAPMode();
        return;  // loop() handles AP from here
    }

    drawScan();
    fetchAndUpdate();
    render();
}

void loop() {
    // AP mode: serve web requests only
    if (inAPMode) {
        apServer.handleClient();
        return;
    }

    M5.update();
    apServer.handleClient();

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
            uint32_t now = millis();
            if (now - lastClickMs <= DOUBLE_CLICK_MS) {
                lastClickMs = 0;  // reset so triple-click doesn't re-trigger
                sendTestNtfy();
            } else {
                lastClickMs = now;
                int maxScroll = (int)debugLines.size() - DBG_LINES_VISIBLE;
                if (maxScroll < 0) maxScroll = 0;
                debugScroll = (debugScroll >= maxScroll) ? 0 : debugScroll + DBG_LINES_VISIBLE;
            }
        } else {
            switch (mode) {
                case SCR_SCAN:
                    mode = (histCount > 0) ? SCR_HIST : SCR_SUM;
                    break;
                case SCR_HIST:
                    if (histIdx < histCount - 1) {
                        histIdx++;   // show next older entry
                    } else {
                        histIdx = 0; // reset for next visit
                        mode = SCR_SUM;
                    }
                    break;
                case SCR_SUM:  mode = SCR_SCAN; break;
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
    if (millis() - lastPoll >= appCfg.pollMs) {
        lastPoll = millis();
        fetchAndUpdate();
        render();
    }
}
