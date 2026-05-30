# atom-plane-tracker

ESP32-S3 firmware for the **M5Stack Atom S3R** that connects to a public ADS-B API and displays live aircraft flying within a configurable radius of a fixed coordinate. No API key required.

---

## What it does

- Connects to a pre-configured WiFi network on boot.
- Polls [airplanes.live](https://airplanes.live) (`api.adsb.one`) every 10 seconds for aircraft within **5 nautical miles** of a stored coordinate.
- Classifies each aircraft and changes the screen color accordingly:

| Class | Background | Text | Detection |
|---|---|---|---|
| Military | Red | Black | `mil` flag, `dbFlags`, or known callsign prefix |
| Medevac / Air Life | Blue | White | LIFEGRD/MEDVAC callsign prefix or operator name |
| Commercial | Green | Black | Heavy/large ADS-B category or ICAO airline code |
| Private / Other | Yellow | Black | Everything else |

- Shows callsign, aircraft type (ICAO type code), and owner/operator.
- When an aircraft leaves the radius the screen returns to **SCANNING**.
- **Button** (front of device) cycles through three screens when no aircraft is overhead:

```
SCANNING → HISTORY → SUMMARY → SCANNING → ...
```

- **HISTORY** — last aircraft detected, shown with a `[HIST]` tag.
- **SUMMARY** — session totals (e.g. 2 Military, 1 Commercial, 3 Private).
- A new aircraft detected while viewing HISTORY or SUMMARY will interrupt and take over the display, then return to SCANNING when it leaves.

---

## Hardware

- [M5Stack Atom S3R](https://shop.m5stack.com/products/atoms3r-dev-kit) (ESP32-S3, 0.85" 128×128 IPS LCD)
- USB-C cable for flashing

---

## Prerequisites

### Software

| Tool | Notes |
|---|---|
| [VS Code](https://code.visualstudio.com/) | Any recent version |
| [PlatformIO IDE extension](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide) | Install from VS Code Extensions panel |

PlatformIO will automatically download the ESP32 platform toolchain and all library dependencies (`M5Unified`, `ArduinoJson`) on first build — no manual installs needed.

---

## Setup

### 1. Clone the repo

```bash
git clone https://github.com/your-username/atom-plane-tracker.git
cd atom-plane-tracker
```

### 2. Create your secrets file

Copy the example and fill in your values:

```bash
cp include/secrets.h.example include/secrets.h
```

Edit `include/secrets.h`:

```cpp
#define WIFI_SSID       "YourNetworkName"
#define WIFI_PASSWORD   "YourNetworkPassword"

#define QUERY_LAT        36.0000   // decimal degrees, positive = North
#define QUERY_LON       -84.0000   // decimal degrees, negative = West

#define QUERY_RADIUS_NM  5.0       // nautical miles (5 NM ≈ 5.75 statute miles)
```

> `secrets.h` is listed in `.gitignore` and will never be committed.

---

## Compile

1. Open the `atom-plane-tracker` folder in VS Code (`File → Open Folder`).
2. PlatformIO will detect `platformio.ini` automatically.
3. Click the **checkmark (Build)** icon in the PlatformIO toolbar at the bottom of the window, or open the Command Palette (`Ctrl+Shift+P`) and run **PlatformIO: Build**.

First build will take a few minutes while the toolchain and libraries download.

---

## Flash

1. Connect the Atom S3R via USB-C.
2. Click the **right-arrow (Upload)** icon in the PlatformIO toolbar, or run **PlatformIO: Upload** from the Command Palette.
3. PlatformIO will compile (if needed), find the serial port automatically, and flash the device.
4. The device will reboot and connect to WiFi immediately after flashing.

> If the port is not detected, press and hold the reset button on the Atom S3R for 2 seconds before clicking Upload.

### Monitor serial output (optional)

Click the **plug icon (Serial Monitor)** in the PlatformIO toolbar to open a terminal at 115200 baud. Useful for debugging WiFi connection and API responses.

---

## API

Data is sourced from **[airplanes.live](https://airplanes.live)** via their free REST API hosted at `api.adsb.one`. No account or API key is required. The endpoint used is:

```
GET https://api.adsb.one/v2/point/{lat}/{lon}/{radius_nm}
```

A commented fallback URL for [adsb.lol](https://adsb.lol) (identical response format) is included in `src/main.cpp`.

---

## Project structure

```
atom-plane-tracker/
├── platformio.ini          # Build config — ESP32-S3, M5Unified, ArduinoJson
├── src/
│   └── main.cpp            # All firmware logic
├── include/
│   ├── secrets.h           # Your credentials — gitignored, never committed
│   └── secrets.h.example   # Safe template to commit and share
└── .gitignore
```
