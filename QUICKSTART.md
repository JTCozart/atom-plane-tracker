# Quick Start Guide

This guide walks you through setting up your **atom-plane-tracker** if it came preloaded with default settings.

---

## What you'll need

- **atom-plane-tracker device** (M5Stack Atom S3R)
- **Smartphone** (iOS or Android)
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

This means the device is in setup mode and waiting for you to connect it to your WiFi.

---

## Step 2: Connect your phone/laptop to the device's WiFi

On your phone or laptop:

1. Open your WiFi settings
2. Look for a network called **`PlaneTracker`**
3. Select it and enter the password: **`PlaneTracker`**
4. Your device is now connected to the tracker itself (not the internet yet)

---

## Step 3: Open the setup page in your browser

1. Open any web browser (Chrome, Safari, Firefox)
2. Go to: **`http://192.168.4.1`**
3. You should see a form titled **"PlaneTracker Setup"**

If the page doesn't load, try refreshing (Ctrl+R or Cmd+R).

---

## Step 4: Connect to your WiFi

On the setup form, fill in:

| Field | What to enter |
|---|---|
| **WiFi SSID** | The name of your home WiFi network |
| **WiFi Password** | Your WiFi password |

Click **Save & Reboot**.

The device will:
- Save your WiFi settings
- Reboot automatically
- Connect to your home WiFi (this takes ~10 seconds)
- Show the scanning screen

---

## Step 5: Find your location coordinates

You need to set the **latitude** and **longitude** where you want to track aircraft.

### Quick method (use your current location):

1. Go to **Google Maps** on your phone
2. Tap your current location (the blue dot)
3. The popup shows your coordinates at the top: **`lat, lon`**
4. Example: `36.1234, -83.9876`

### For a specific address:

1. Go to **Google Maps**
2. Search for an address
3. Right-click (or long-press on mobile) on the map
4. Select the coordinates that appear at the top

---

## Step 6: Find the device's IP address

The device shows its IP address on the debug screen. Here's how:

1. Look at the device screen (should still be in setup/scanning mode)
2. **Long-press the button for 0.8 seconds** (hold it down)
3. The screen changes to show debug info with the IP address on line 2:

```
DBG HTTP:200 ac:7
IP: 192.168.1.XX
[rest of debug info...]
```

4. Write down the **IP address** (e.g., `192.168.1.50`)

5. **Long-press the button again** to exit debug and return to scanning

6. Open your web browser and go to: **`http://192.168.1.XX`** (use the IP you found)

---

## Step 7: Set your location and preferences

On the device's setup page, update these fields:

| Field | What to enter |
|---|---|
| **Latitude** | Your latitude (from Google Maps) |
| **Longitude** | Your longitude (from Google Maps) |
| **Search Radius (NM)** | How far to search for aircraft (nautical miles). Default: 10 NM ≈ 11.5 statute miles |
| **Poll Interval (ms)** | How often to check for aircraft (milliseconds). Default: 15000 = 15 seconds. **Must be at least 10000 (10 seconds)** |

Click **Save & Reboot**.

The device will now start scanning for aircraft overhead!

---

## Step 8 (Optional): Set up push notifications

If you want your phone to notify you when aircraft pass overhead:

### 8a. Create a ntfy.sh account

1. Go to **https://ntfy.sh** on your phone
2. You **don't need an account** — ntfy is anonymous and free
3. But if you want to keep your topic private, create a topic name:
   - Think of something **unique** that no one else would guess
   - Example: `airplane-tracker-myname-12345`
   - Write it down — you'll need it twice

### 8b. Download the ntfy app on your phone

- **iPhone**: Search for **"ntfy"** in the App Store, install the official app
- **Android**: Search for **"ntfy"** in Google Play, install the official app

### 8c. Create a topic in the ntfy app

1. Open the **ntfy app** on your phone
2. Tap **"+"** or **"Add subscription"**
3. Enter your **unique topic name** from step 8a (e.g., `airplane-tracker-myname-12345`)
4. Subscribe — the app will now listen for notifications on that topic

### 8d. Generate an access token

1. Go back to **https://ntfy.sh** in your browser
2. Scroll down to the **"Publish"** section
3. Look for instructions on creating an **access token** or click your topic name at the top
4. Copy the token (it looks like: `tk_u2f0538r6kubswdvvmnpcf1bcap7v`)

### 8e. Add ntfy settings to the device

1. Go back to your device's setup page: **`http://192.168.1.XX`** (the IP you found in Step 6)
2. Scroll down to the **"ntfy"** section
3. Fill in:

| Field | What to enter |
|---|---|
| **ntfy Token** | The access token from step 8d |
| **ntfy Topic** | Your unique topic name (e.g., `airplane-tracker-myname-12345`) |
| **ntfy Classes** | (Optional) Which aircraft to notify about: `MIL,MEDVAC,COMM,PRIV` (empty = all) |

4. Click **Save & Reboot**

Now, whenever an aircraft enters your search radius, you'll get a notification on your phone with a **FlightRadar24** button that opens the live flight.

---

## Step 9: You're done!

The device is now:
- ✅ Connected to your WiFi
- ✅ Scanning for aircraft over your location
- ✅ (Optional) Sending notifications to your phone

### What to expect

- **Black screen with "SCANNING"**: The device is listening for aircraft
- **Colored screen with flight details**: An aircraft is overhead! Shows callsign, type, altitude, and how long until it leaves your area
- **Animated radar sweep**: Smooth animation showing the device is actively scanning (not stuck)

### Changing settings later

If you ever want to change your WiFi, location, or notification settings:

1. Find the device's IP address (from the debug screen, or your router's connected devices list)
2. Open **`http://192.168.1.XX`** in your browser
3. Update any fields you want
4. Click **Save & Reboot**

---

## Troubleshooting

### The device won't connect to WiFi

- Make sure you entered the SSID and password correctly
- Try restarting your WiFi router
- Move the device closer to the router

### Can't find the setup page

- Make sure you're connected to the **PlaneTracker** WiFi (not your home WiFi)
- Try `192.168.4.1` again, or refresh (Ctrl+R)
- Try a different browser

### Not getting notifications

- Make sure your **ntfy Topic** matches exactly what you subscribed to in the app
- Check the debug screen (long-press button) to see if the device is online
- Try sending a test notification: long-press the button twice on the debug screen

### Can't find the device's IP after setup

- Long-press the button on the device to enter debug mode — the IP is always shown on line 2
- If the device won't turn on or the screen is broken, power it off, wait 5 seconds, and power it back on

---

## Questions?

If something isn't working, check the **debug screen** on the device (long-press the button for 0.8 seconds). It shows:
- **HTTP status code** and **aircraft count** from the last API check
- **Device IP address** for accessing the setup page
- **Raw data** from the API response

Good luck tracking! 🛩️
