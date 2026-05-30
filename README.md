# atom-plane-tracker

ESP32-S3 firmware for the **M5Stack Atom S3R** that connects to a free public ADS-B API and displays live aircraft flying within a configurable radius of a fixed coordinate. No API key required.

---

## What it does

- Connects to a pre-configured WiFi network on boot and immediately polls for aircraft.
- Polls [adsb.lol](https://adsb.lol) every 15 seconds (configurable) for aircraft within a stored radius of a fixed coordinate.
- Classifies each aircraft and changes the screen color accordingly:

| Class | Background | Text | Basis |
|---|---|---|---|
| Military | Red | Black | API `mil` flag or `dbFlags` bit 0 |
| Medevac / Air Life | Blue | White | FAA LIFEGUARD/MEDVAC callsign prefix or known operator |
| Commercial | Green | Black | ADS-B category A3/A4/A5 or ICAO airline code pattern |
| Private / Other | Yellow | Black | Everything else |

- Detection screen shows **callsign**, **aircraft type**, **altitude**, **ETA until leaving radius**, and **owner/operator**.
- ETA counts down in real time between API polls using the aircraft's last known position, speed, and track.
- If multiple aircraft are overhead simultaneously the display cycles between them every 5 seconds.
- When all aircraft leave the radius the screen returns to **SCANNING**.

---

## Screen navigation

Short press of the button cycles through three screens. This works at any time — including while aircraft are overhead.

```
SCANNING  →  HISTORY  →  SUMMARY  →  SCANNING  → ...
```

| Screen | Description |
|---|---|
| **SCANNING** | Black screen with "SCANNING". When aircraft are overhead this shows the live detection screen instead. |
| **HISTORY** | Last 5 detected aircraft. Each press advances to the next older entry (`[ HIST 1/3 ]` shown in the red bar). After the last entry one more press moves to SUMMARY. |
| **SUMMARY** | Session totals by class (Military, Medevac, Commercial, Private). |

- A new aircraft detected while on HISTORY or SUMMARY interrupts to the live view and returns to SCANNING when it leaves.
- HISTORY and SUMMARY auto-return to SCANNING after **30 seconds** of no button activity.

---

## Debug mode

**Long press (0.8 s)** on the button from any screen enters debug mode. Long press again to exit and return to the previous screen.

The debug screen shows raw data from the last API response:

```
DBG HTTP:200 ac:7
A1B2C3 UAL123 B738
 cat:A3 mil:N 35000ft
 United Airlines
---
D4E5F6 N12345 C172
 cat:A1 mil:N  2500ft
---
                4/12
```

- **Header** — last HTTP status code and total aircraft count in the API response.
- **Per aircraft** — ICAO hex, callsign or registration, ICAO type code, ADS-B category, military flag (`Y`/`N`), barometric altitude, and owner/operator.
- **Short press** — scrolls down 15 lines at a time, wraps to top.
- **Double short press** (two taps within 400 ms) — sends a test ntfy notification and displays the HTTP response code for 1.5 seconds. Shows `ntfy not configured` if `NTFY_TOPIC` is empty.
- Scroll position is preserved while in debug. Aircraft arrivals and departures do not exit debug mode.

---

## Push notifications (optional)

The firmware can send push notifications via [ntfy.sh](https://ntfy.sh) when an aircraft enters the radius. Priority and tags vary by class:

| Class | Priority | Tag |
|---|---|---|
| Military | Urgent | 🚨 |
| Medevac | High | 🚑 |
| Commercial | Default | ✈️ |
| Private | Low | 🛩️ |

Configure `NTFY_TOKEN` and `NTFY_TOPIC` in `secrets.h`. Leave `NTFY_TOPIC` as an empty string to disable notifications entirely.

---

## Hardware

- [M5Stack Atom S3R](https://shop.m5stack.com/products/atoms3r-dev-kit) (ESP32-S3, 0.85" 128×128 IPS LCD)
- USB-C cable for flashing

---

## Prerequisites

| Tool | Notes |
|---|---|
| [VS Code](https://code.visualstudio.com/) | Any recent version |
| [PlatformIO IDE extension](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide) | Install from the VS Code Extensions panel |

PlatformIO automatically downloads the ESP32 toolchain and all library dependencies (`M5Unified`, `ArduinoJson`) on first build.

---

## Setup

### 1. Clone the repo

```bash
git clone https://github.com/your-username/atom-plane-tracker.git
cd atom-plane-tracker
```

### 2. Create your secrets file

```bash
cp include/secrets.h.example include/secrets.h
```

Edit `include/secrets.h`:

```cpp
#define WIFI_SSID        "YourNetworkName"
#define WIFI_PASSWORD    "YourNetworkPassword"

#define QUERY_LAT         36.0000   // decimal degrees, positive = North
#define QUERY_LON        -84.0000   // decimal degrees, negative = West
#define QUERY_RADIUS_NM   5.0       // nautical miles (5 NM ≈ 5.75 statute miles)

#define POLL_INTERVAL_MS  15000     // API poll interval in milliseconds

// Optional — leave NTFY_TOPIC empty to disable
#define NTFY_TOKEN  "your-ntfy-access-token"
#define NTFY_TOPIC  "your-topic-name"
```

> `secrets.h` is listed in `.gitignore` and will never be committed.

---

## Compile

1. Open the `atom-plane-tracker` folder in VS Code (`File → Open Folder`).
2. PlatformIO detects `platformio.ini` automatically.
3. Click the **checkmark (Build)** icon in the PlatformIO toolbar, or run **PlatformIO: Build** from the Command Palette (`Ctrl+Shift+P`).

First build takes a few minutes while the toolchain and libraries download.

---

## Flash

1. Connect the Atom S3R via USB-C.
2. Click the **right-arrow (Upload)** icon in the PlatformIO toolbar, or run **PlatformIO: Upload** from the Command Palette.
3. PlatformIO compiles (if needed), finds the serial port automatically, and flashes the device.
4. The device reboots, connects to WiFi, and polls immediately.

> If the port is not detected, hold the reset button on the Atom S3R for 2 seconds before clicking Upload.

### Monitor serial output (optional)

Click the **plug icon (Serial Monitor)** in the PlatformIO toolbar to open a terminal at 115200 baud.

---

## API

Data comes from **[adsb.lol](https://adsb.lol)** — a community-run, free ADS-B feed with no account or API key required. The endpoint used is:

```
GET https://api.adsb.lol/v2/lat/{lat}/lon/{lon}/dist/{radius_nm}
```

A commented fallback URL for [airplanes.live](https://airplanes.live) (`api.adsb.one`) is in `src/main.cpp` — identical response format, swap by uncommenting.

---

## Project structure

```
atom-plane-tracker/
├── platformio.ini          # Build config — m5stack-atoms3, M5Unified, ArduinoJson
├── src/
│   └── main.cpp            # All firmware logic
├── include/
│   ├── secrets.h           # Your credentials — gitignored, never committed
│   └── secrets.h.example   # Safe template
└── .gitignore
```
