---
name: project-atom-plane-tracker
description: New firmware project for the M5Stack Atom S3R (ESP32-S3) to track aircraft over given coordinates using a public ADS-B/air traffic API and display info on the built-in display.
metadata:
  type: project
---

Project: atom-plane-tracker (ESP32 firmware)

**Why:** Display live aircraft overhead info on the Atom S3R's small screen, pulling from a public air traffic API over WiFi.

**How to apply:** When suggesting architecture, target the Atom S3R hardware constraints (ESP32-S3, small 0.85" 128x128 LCD, limited RAM). Likely PlatformIO + Arduino framework. API likely OpenSky Network or ADSB.lol (free, no key needed). More instructions from user are pending.

Repo path: C:\Users\jtczrt\Documents\GitHub\atom-plane-tracker
