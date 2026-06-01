# Quick Start Guide

This guide walks you through flashing and configuring your **atom-plane-tracker** on a brand-new M5Stack Atom S3R straight from the factory.

---

## What you'll need

- **M5Stack Atom S3R** (with USB-C cable)
- **Smartphone or laptop**
- **WiFi network** at your location
- **Chrome, Edge, or any Chromium browser** (required for the flash step)
- **10 minutes**

---

## Step 1: Flash the firmware

The Atom S3R ships blank. You need to load the tracker firmware once using a browser-based tool - no software installation required.

### 1a. Download the firmware

1. Go to the [latest release](https://github.com/JTCozart/atom-plane-tracker/releases/latest)
2. Download the file ending in **`-initial-flash.bin`** (e.g. `atom-plane-tracker-v20260531.1234-initial-flash.bin`)

> Download the `-initial-flash.bin` file, not the plain `.bin`. The initial-flash version includes the bootloader and is required for first-time setup.

### 1b. Open the flash tool

1. In **Chrome or Edge**, go to: **[https://adafruit.github.io/Adafruit_WebSerial_ESPTool/](https://adafruit.github.io/Adafruit_WebSerial_ESPTool/)**
2. Connect the Atom S3R to your computer via USB-C

### 1c. Connect and flash

1. Click **Connect** in the top-right corner
2. Select the serial port for your device (it will appear as a USB serial device)
3. Click **Erase** to wipe the chip, then wait for it to complete
4. Under **Program**, set the address to **`0x0`**
5. Click **Choose a file** and select the `-initial-flash.bin` you downloaded
6. Click **Program** and wait until it says complete

The device will reboot automatically. Because the firmware's default `secrets.h` has `WIFI_SSID = "SETUP"`, it skips the WiFi connection attempt and goes straight to setup mode. The screen will show:

```
SETUP MODE
SSID: PlaneTracker
Pass: PlaneTracker
```

---

## Step 2: Connect to the device's WiFi hotspot

On your phone or laptop:

1. Open your WiFi settings
2. Connect to the network called **`PlaneTracker`**, password **`PlaneTracker`**
3. You are now connected directly to the tracker (not the internet yet)

---

## Step 3: Open the setup page

1. Open any web browser
2. Go to: **`http://192.168.4.1`**
3. You should see the **PlaneTracker Setup** page

If the page doesn't load, try refreshing (Ctrl+R or Cmd+R).

---

## Step 4: Configure all your settings

Fill in all tabs before saving - this way the device only needs to reboot once.

### WiFi tab

| Field | What to enter |
|---|---|
| **SSID** | The name of your home WiFi network |
| **Password** | Your WiFi password |

### Detection tab

| Field | What to enter |
|---|---|
| **Latitude / Longitude** | Your location - use the **Pick on map** button to drop a pin instead of typing coordinates manually |
| **Search Radius (NM)** | How far to scan for aircraft in nautical miles. 10 NM is a good starting point |
| **Scan Interval (s)** | How often to poll for aircraft in seconds. Default: 15. **Minimum: 10** |

> **Tip:** Before saving, click the **API Test** tab and press **Run Test**. The device fires a live query using the coordinates and radius you entered and shows the raw JSON response. If you see aircraft you're all set; if it's empty try increasing the radius.

### Notifications tab (optional)

See [Step 6](#step-6-optional-set-up-push-notifications) for how to get an ntfy token and topic - you can always come back to this after the initial setup.

---

## Step 5: Save & find your IP address

Click **Save & Reboot**. The device saves all settings, reboots, and joins your home WiFi - this takes about 10-15 seconds.

**Switch your phone/laptop back to your home WiFi network.**

Now find the device's new IP address:

1. **Long-press the button on the device for 0.8 seconds** to enter debug mode
2. The screen shows:

```
DBG HTTP:200 ac:7
IP: 192.168.1.XX
UP: 00:00:42
...
```

3. Note the IP address on line 2 (e.g. `192.168.1.50`)
4. **Long-press again** to exit debug and return to scanning
5. Open **`http://192.168.1.XX`** in your browser - this is your settings page from now on

> **Bookmark it.** Any time you want to change settings, open this address in your browser. You can also find the IP from your router's connected devices list if you forget it.

---

## Step 6 (Optional): Set up push notifications

If you want your phone to alert you when aircraft pass overhead:

### About ntfy.sh (free service)

- **Free tier** - 250 notifications per day, no credit card required
- **Private topics** - use a unique name nobody else would guess

### 6a. Download the ntfy app

- **iPhone**: Search **"ntfy"** in the App Store
- **Android**: Search **"ntfy"** in Google Play

### 6b. Create an account and topic on ntfy.sh

1. Go to **https://ntfy.sh** and create a free account
2. Create a **unique topic name** (e.g. `planetracker-yourname-12345`) and subscribe to it in the app
3. In your account settings, generate an **access token** - it looks like `tk_u2f0538r6kubswdvvmnpcf1bcap7v`

### 6c. Add the token and topic to the device

1. Open the settings page: **`http://192.168.1.XX`**
2. Click the **Notifications** tab
3. Fill in:

| Field | What to enter |
|---|---|
| **ntfy Token** | Your access token from step 6b |
| **ntfy Topic** | Your unique topic name |
| **ntfy Notification Categories** | Which events trigger alerts - leave all class checkboxes unchecked to notify for everything. **Emergency Squawk** (checked by default) sends an urgent alert when an aircraft squawks 7500, 7600, or 7700. **Firmware Updates** (checked by default) sends a push when new firmware is available. |

4. Click **Save & Reboot**

Whenever an aircraft enters your radius you'll get a push notification with a **Track Flight** button that opens the live flight on ADS-B Exchange.

> **Tip:** After saving, open the Notifications tab and check the **Account Usage** box to confirm your remaining daily quota.

---

## Step 7: You're done!

The device is now scanning for aircraft over your location. Here's what to expect:

- **Animated radar sweep** - actively scanning, no aircraft in range yet
- **Colored screen with flight info** - an aircraft is overhead; shows callsign, type, altitude, ETA, and squawk code (emergency codes flash red)
- The web UI's live preview mirrors the device screen and refreshes every 5 seconds

### Future firmware updates

The device checks for new firmware releases automatically every 24 hours. If you enabled **Updates** notifications you'll get a push alert when one is available. You can also check manually in the **Update** tab of the settings page and install with one click - no computer required.

### Coming back to change settings

Open **`http://192.168.1.XX`** (the IP from Step 5) in any browser on your home network, make your changes, and click **Save & Reboot**.

---

## Troubleshooting

### Device won't connect to WiFi after setup

The device falls back to setup mode (broadcasting `PlaneTracker` hotspot) if it can't reach the network. Reconnect to `192.168.4.1` and double-check the SSID and password. Note: if the SSID is still set to `SETUP` the device will always boot directly into setup mode — make sure you saved a real network name.

### Can't find `192.168.4.1`

- Make sure you're connected to the **PlaneTracker** WiFi, not your home network
- Try refreshing or a different browser

### Can't find the device's IP after it joins home WiFi

- Long-press the button on the device - IP is on line 2 of the debug screen
- Check the connected devices list in your router's admin page

### Not getting notifications

- Make sure the **ntfy Topic** on the device matches exactly what you subscribed to in the app
- Use the **Send Test Notification** button on the Notifications tab - it reports the HTTP response code so you can see if the token is valid

### Debug screen shows HTTP error or no aircraft ever appear

- Long-press the button to enter debug mode and check the HTTP status code on line 1
- Use the **API Test** tab to run a live query and inspect the raw response
- Try increasing the search radius

---

## Questions?

Long-press the button (0.8 s) to open the debug screen at any time. It shows:
- **Line 1** - last HTTP status code and aircraft count
- **Line 2** - device IP address
- **Line 3** - uptime since last boot
- **Remaining lines** - raw data from the last API response

Good luck tracking!
