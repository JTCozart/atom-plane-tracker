# atom-plane-tracker

ESP32-S3 firmware for the **M5Stack Atom S3R** that connects to a free public ADS-B API and displays live aircraft flying within a configurable radius of a fixed coordinate. No API key required.

---

## What it does

- Connects to a pre-configured WiFi network on boot and immediately scans for aircraft.
- Polls [adsb.lol](https://adsb.lol) for aircraft within a configurable radius of a fixed coordinate (scan interval is adjustable).
- Classifies each aircraft and changes the screen color accordingly:

| Class | Background | Text | Basis |
|---|---|---|---|
| Military | Red | Black | API `mil` flag or `dbFlags` bit 0 |
| Medevac / Air Life | Blue | White | FAA LIFEGUARD/MEDVAC/REACH/LIFEFLT callsign prefix, or owner contains EMS/LIFE FLIGHT/AIR EVAC/AIR METHODS/PHI AIR and others |
| Commercial | Green | Black | ADS-B category A3/A4/A5 or ICAO airline code pattern |
| Private / Other | Yellow | Black | Everything else |

- Detection screen shows **callsign**, **aircraft type**, **altitude**, and **ETA until leaving radius**.
- ETA counts down in real time between API polls using the aircraft's last known position, speed, and track.
- If multiple aircraft are overhead simultaneously the display cycles between them every 5 seconds.
- When all aircraft leave the radius the screen returns to **SCANNING** with an animated radar sweep.

---

## Screen navigation

| Button Action | Effect |
|---|---|
| **Short press** | Cycle through screens: SCANNING → HISTORY → SUMMARY → SCANNING |
| **Long press** (0.8 s) | Enter/exit DEBUG mode |

Screen details:

| Screen | Description |
|---|---|
| **SCANNING** | Animated radar sweep showing scanning is active. When aircraft are overhead this shows the live detection screen instead. |
| **HISTORY** | Last 5 detected aircraft. Each press advances to the next older entry - `[ HIST 1/5 ]` counter shown in the red bar at the bottom. After the last entry one more press moves to SUMMARY. |
| **SUMMARY** | Session totals by class (Military, Medevac, Commercial, Private). |

- A new aircraft detected while on HISTORY or SUMMARY interrupts to the live view and returns to SCANNING when it leaves.
- HISTORY and SUMMARY auto-return to SCANNING after **30 seconds** of no button activity.

---

## Debug mode

**Long press (0.8 s)** on the button from any screen enters debug mode. Long press again to exit and return to the previous screen.

The debug screen shows device status and raw data from the last API response:

```
DBG HTTP:200 ac:7
IP: 192.168.1.42
UP: 00:42:17
A1B2C3 UAL123 B738
 cat:A3 mil:N 35000ft
---
D4E5F6 N12345 C172
 cat:A1 mil:N  2500ft
---
                3/10
```

### Debug screen details

- **Line 1** - last HTTP status code and total aircraft count returned by the API.
- **Line 2** - device IP address on the local network (see [Configuration web UI](#configuration-web-ui)).
- **Line 3** - uptime since last boot (`HH:MM:SS`).
- **Per aircraft** - ICAO hex, callsign or registration, ICAO type code, ADS-B category, military flag (`Y`/`N`), and barometric altitude.
- **Short press** - scrolls down 12 lines at a time, wraps to top.
- **Double short press** (two taps within 400 ms) - sends a test ntfy notification and displays the HTTP response code for 1.5 seconds. Shows `ntfy not configured` if `NTFY_TOPIC` is empty.
- Scroll position is preserved while in debug. Aircraft arrivals and departures do not exit debug mode.

---

## Configuration web UI

The device runs a built-in web server that lets you update settings without reflashing.

### First run / unconfigured

If WiFi connection fails (wrong credentials or blank `secrets.h`), the device starts a setup access point:

| | |
|---|---|
| **SSID** | `PlaneTracker` |
| **Password** | `PlaneTracker` |
| **URL** | `http://192.168.4.1` |

The screen shows `SETUP MODE` with the connection details.

### While running on WiFi

Once connected the web server stays active on the device's normal IP address. Open a browser on any device on the same network and go to the IP shown on the debug screen (e.g. `http://192.168.1.42`).

Both modes serve the same settings page, organised into four tabs:

**WiFi tab**

| Field | Description |
|---|---|
| SSID | Network name |
| Password | Leave blank to keep the current password |

**Detection tab**

| Field | Description |
|---|---|
| Latitude / Longitude | Center point for aircraft queries (decimal degrees). Use the **Pick on map** button to drop a pin on an interactive map instead of typing coordinates. |
| Search Radius (NM) | Query radius in nautical miles |
| Scan Interval (s) | How often to scan for aircraft, in seconds. **Minimum: 10 seconds** |

**Notifications tab**

| Field | Description |
|---|---|
| ntfy Token | Leave blank to keep the current token |
| ntfy Topic | Leave blank to disable notifications |
| ntfy Notification Categories | Checkbox dropdown - select which events trigger notifications. Aircraft classes: Military, Medevac, Commercial, Private. Leave all unchecked to notify for all classes. **Updates** (on by default) sends a push when new firmware is available. |

An **Account Usage** box at the bottom of the Notifications tab shows messages sent, the account limit, and remaining quota for the current billing period (fetched live from `ntfy.sh/v1/account`). It loads automatically when the tab is opened and has a Refresh button.

**API Test tab**

Runs a live query to adsb.lol using the lat/lon/radius currently entered in the Detection tab (no need to save first). The raw JSON response is pretty-printed in a scrollable green-on-black terminal window. Useful for verifying coordinates and radius before committing them.

**Save & Reboot** submits all values to on-device flash (NVS) and reboots. **NVS values take priority over `secrets.h`** on every subsequent boot.

**Clear All Settings** (red button) erases all saved preferences and resets to `secrets.h` defaults. Shows a confirmation dialog to prevent accidental clearing.

After saving or clearing, the browser waits for the device to come back online and returns to the settings page automatically. If the device does not reappear within 30 seconds (e.g. WiFi credentials changed) a notice appears explaining how to connect via the setup hotspot.

### Live screen preview

The settings page includes a live rendering of the device screen in a panel alongside the settings form. It refreshes every 5 seconds and shows the current mode - SCANNING, live aircraft, HISTORY, SUMMARY, or DEBUG - with colors matching the physical display. The SCANNING state renders the same animated radar sweep as the device display. Control buttons below the preview switch the device between modes in real time.

### Other features

- **Dark mode** - toggle in the top-right corner; defaults to your system preference and persists per browser.
- **Send Test Notification** - button in the Notifications tab sends a test ntfy push and reports the HTTP result inline without saving or rebooting.

---

## Push notifications (optional)

The firmware can send push notifications via [ntfy.sh](https://ntfy.sh) when an aircraft enters the radius. Priority and tags vary by class:

| Class | Priority | Tag |
|---|---|---|
| Military | Urgent | 🚨 |
| Medevac | High | 🚑 |
| Commercial | Default | ✈️ |
| Private | Low | 🛩️ |

Each notification includes a **Track Flight** action button that opens the live flight on [ADS-B Exchange](https://globe.adsbexchange.com) using the aircraft's ICAO hex code. All notifications use a radar dish icon (📡).

A separate **firmware update** notification is sent when a new release is detected (arrow_up tag, default priority). This can be toggled off under **ntfy Notification Categories** in the web UI.

Configure in `secrets.h` (compile-time defaults) or via the **configuration web UI** (saved to NVS, takes priority):

```cpp
#define NTFY_TOKEN   "your-access-token"
#define NTFY_TOPIC   "your-topic-name"
// Comma-separated filter: MIL, MEDVAC, COMM, PRIV - empty = all classes
#define NTFY_CLASSES "MIL,MEDVAC"
```

Leave `NTFY_TOPIC` empty to disable notifications entirely. Test notifications can be sent via the **Send Test Notification** button in the web UI, or by **double-pressing** the button on the debug screen - both report the HTTP response code.

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

Edit `include/secrets.h` with your defaults. These values are used on first boot and as fallbacks if NVS is empty:

```cpp
#define WIFI_SSID        "YourNetworkName"
#define WIFI_PASSWORD    "YourNetworkPassword"

#define QUERY_LAT         36.0000   // decimal degrees, positive = North
#define QUERY_LON        -84.0000   // decimal degrees, negative = West
#define QUERY_RADIUS_NM   5.0       // nautical miles (5 NM ≈ 5.75 statute miles)

#define POLL_INTERVAL_MS  15000     // milliseconds between API polls

// Optional ntfy notifications - leave NTFY_TOPIC empty to disable
#define NTFY_TOKEN   "your-ntfy-access-token"
#define NTFY_TOPIC   "your-topic-name"
#define NTFY_CLASSES ""             // empty = all classes
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

Data comes from **[adsb.lol](https://adsb.lol)** - a community-run, free ADS-B feed with no account or API key required. The endpoint used is:

```
GET https://api.adsb.lol/v2/lat/{lat}/lon/{lon}/dist/{radius_nm}
```

---

## Flashing the firmware

Each [GitHub release](https://github.com/JTCozart/atom-plane-tracker/releases) includes two binaries:

| File | Use |
|---|---|
| `atom-plane-tracker-vXXX.bin` | OTA updates - downloaded automatically by the device |
| `atom-plane-tracker-vXXX-initial-flash.bin` | **First-time install** - single merged binary, flash at address `0x0` |

### First-time install (no software required)

1. Download `atom-plane-tracker-vXXX-initial-flash.bin` from the latest release
2. Open **[Adafruit WebSerial ESPTool](https://adafruit.github.io/Adafruit_WebSerial_ESPTool/)** in Chrome (or any Chromium browser)
3. Connect the M5Stack Atom S3 via USB
4. Click **Connect**, select the serial port, then **Program**
5. Set the address to `0x0` and select the `initial-flash.bin` file
6. Click **Program** and wait for it to complete

After the first flash, all future updates can be applied wirelessly from the **Update** tab in the web UI, or the device checks automatically every 24 hours and sends an ntfy notification when a new release is available.

### Building from source

See [QUICKSTART.md](QUICKSTART.md) and the [Development](#development) section below.

---

## Project structure

```
atom-plane-tracker/
├── platformio.ini              # Build config - m5stack-atoms3, M5Unified, ArduinoJson
├── README.md                   # This file
├── QUICKSTART.md               # Quick-start guide for new users
├── src/
│   ├── main.cpp                # Setup, loop, button/display logic
│   ├── Aircraft.h/cpp          # Aircraft classification & ETA calculation
│   ├── AircraftStore.h/cpp     # Aircraft state, history, API polling
│   ├── AppState.h              # Screen mode enum
│   ├── Config.h/cpp            # Configuration struct, NVS persistence
│   ├── Display.h/cpp           # All display drawing (including animated scanning)
│   ├── Notifier.h/cpp          # ntfy.sh push notifications
│   ├── OtaUpdater.h/cpp        # GitHub release check and OTA firmware update
│   └── WebUI.h/cpp             # HTTP server, configuration web UI
├── include/
│   ├── secrets.h               # Your credentials - gitignored, never committed
│   └── secrets.h.example       # Safe template to copy from
└── .gitignore
```

**Architecture**: Refactored to follow SOLID principles. Thin `main.cpp` orchestrates independent, single-responsibility classes.

---

## Getting started

New to the project and have a factory device? Start with **[QUICKSTART.md](QUICKSTART.md)** - step-by-step from first flash to live aircraft tracking, no software installation required.
