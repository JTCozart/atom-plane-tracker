# Quick Start Guide

This guide walks you through setting up your **atom-plane-tracker** if it came preloaded with default settings.

---

## What you'll need

- **atom-plane-tracker device** (M5Stack Atom S3R)
- **Smartphone or laptop**
- **WiFi network** at your location
- **Internet browser** (Chrome, Safari, Firefox, etc.)
- **5 minutes**

---

## Step 1: Power on the device

Connect the device to power via USB-C. The screen will display:

```
SETUP MODE
SSID: PlaneTracker
Pass: PlaneTracker
```

This means the device is broadcasting its own WiFi hotspot and waiting to be configured.

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

Fill in all three tabs before saving — this way the device only needs to reboot once.

### WiFi tab

| Field | What to enter |
|---|---|
| **SSID** | The name of your home WiFi network |
| **Password** | Your WiFi password |

### Detection tab

| Field | What to enter |
|---|---|
| **Latitude / Longitude** | Your location — use the **Pick on map** button to drop a pin instead of typing coordinates manually |
| **Search Radius (NM)** | How far to scan for aircraft in nautical miles. 10 NM ≈ 11.5 statute miles is a good starting point |
| **Scan Interval (s)** | How often to poll for aircraft in seconds. Default: 15. **Minimum: 10** |

> **Tip:** Before saving, click the **API Test** tab and press **Run Test**. The device will fire a live query using the coordinates and radius you entered and show the raw JSON response. If you see aircraft you're all set; if it's empty try increasing the radius.

### Notifications tab (optional — skip if you don't want push alerts)

See [Step 6](#step-6-optional-set-up-push-notifications) for how to get an ntfy token and topic first — you can always come back to this after the initial setup.

---

## Step 5: Save & find your IP address

Click **Save & Reboot**. The device will save all settings, reboot, and join your home WiFi network — this takes about 10–15 seconds.

**Switch your phone/laptop back to your home WiFi network.**

Now find the device's new IP address so you can access the settings page in the future:

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
5. Open **`http://192.168.1.XX`** in your browser — this is your settings page from now on

> **Bookmark it.** Any time you want to change settings, just open this address in your browser. You can also find the IP from your router's connected devices list if you forget it.

---

## Step 6 (Optional): Set up push notifications

If you want your phone to alert you when aircraft pass overhead:

### About ntfy.sh (free service)

- **Free tier** — 250 notifications per day, no credit card required
- **Private topics** — use a unique name nobody else would guess

### 6a. Download the ntfy app

- **iPhone**: Search **"ntfy"** in the App Store
- **Android**: Search **"ntfy"** in Google Play

### 6b. Create an account and topic on ntfy.sh

1. Go to **https://ntfy.sh** and create a free account
2. Create a **unique topic name** (e.g. `planetracker-yourname-12345`) and subscribe to it in the app
3. In your account settings, generate an **access token** — it looks like `tk_u2f0538r6kubswdvvmnpcf1bcap7v`

### 6c. Add the token and topic to the device

1. Open the settings page: **`http://192.168.1.XX`**
2. Click the **Notifications** tab
3. Fill in:

| Field | What to enter |
|---|---|
| **ntfy Token** | Your access token from step 6b |
| **ntfy Topic** | Your unique topic name |
| **ntfy Categories** | Which aircraft classes trigger alerts — leave all unchecked to notify for everything |

4. Click **Save & Reboot**

Whenever an aircraft enters your radius you'll receive a push notification with a **Track Flight** button that opens the live flight on ADS-B Exchange.

> **Tip:** After saving, open the Notifications tab and check the **Account Usage** box to confirm your remaining daily quota.

---

## Step 7: You're done!

The device is now scanning for aircraft over your location. Here's what to expect:

- **Animated radar sweep** — actively scanning, no aircraft in range yet
- **Colored screen with flight info** — an aircraft is overhead; shows callsign, type, altitude, and ETA until it leaves your radius
- The web UI's live preview mirrors the device screen and refreshes every 5 seconds

### Coming back to change settings

Open **`http://192.168.1.XX`** (the IP from Step 5) in any browser on your home network, make your changes, and click **Save & Reboot**.

---

## Troubleshooting

### Device won't connect to WiFi after setup

- The device will fall back to setup mode (broadcasting `PlaneTracker` hotspot) if it can't reach the network — reconnect to `192.168.4.1` and double-check the SSID and password

### Can't find `192.168.4.1`

- Make sure you're connected to the **PlaneTracker** WiFi, not your home network
- Try refreshing or a different browser

### Can't find the device's IP after it joins home WiFi

- Long-press the button on the device → IP is on line 2 of the debug screen
- Check the connected devices list in your router's admin page

### Not getting notifications

- Make sure the **ntfy Topic** on the device matches exactly what you subscribed to in the app
- Use the **Send Test Notification** button on the Notifications tab — it reports the HTTP response code so you can see if the token is valid

### Debug screen shows HTTP error or no aircraft ever appear

- Long-press the button to enter debug mode and check the HTTP status code on line 1
- Use the **API Test** tab on the settings page to run a live query and inspect the raw response
- Try increasing the search radius

---

## Questions?

Long-press the button (0.8 s) to open the debug screen at any time. It shows:
- **Line 1** — last HTTP status code and aircraft count
- **Line 2** — device IP address
- **Line 3** — uptime since last boot
- **Remaining lines** — raw data from the last API response

Good luck tracking!
