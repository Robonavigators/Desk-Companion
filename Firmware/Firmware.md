# Desk-Companion — Firmware Guide

**Official firmware documentation | Robonavigators**

Desk-Companion is an open-source ESP32-powered desktop companion with 66 facial emotions, an OLED clock, weather forecasts, capacitive touch controls, and a local web dashboard.

This guide explains how to install, configure, update, and troubleshoot the firmware for both supported Seeed Studio XIAO boards.

> **Development status:** The firmware source is available for both boards. Compilation, flash-size compatibility, and complete hardware testing have not yet been independently verified.

---

## 1. Supported Boards

Desk-Companion offers two separate firmware implementations.

| Board | Firmware | Touch GPIO |
|---|---|---|
| XIAO ESP32-C3 | [Desk-Companion-XIAO-ESP32-C3.ino](Firmware/Desk-Companion-XIAO-ESP32-C3.ino) | D1 / GPIO3 |
| XIAO ESP32-S3 | [Desk-Companion-XIAO-ESP32-S3.ino](Firmware/Desk-Companion-XIAO-ESP32-S3.ino) | D10 / GPIO9 |

Both versions provide the same main functionality, with board-specific GPIO and deep-sleep configurations.

**Do not flash the C3 firmware onto the S3 or vice versa.**

## 2. Firmware Features

### OLED and interaction

- 66 selectable facial emotions
- Automatic emotion switching
- Animated eye movement
- Four OLED screens: emotions, clock, weather, and forecast
- Short-touch screen navigation
- Long-touch deep sleep
- Touch-based wake-up
- Startup and Wi-Fi configuration screens

### Networking

- Primary and secondary saved Wi-Fi networks
- Automatic Wi-Fi reconnection
- Wi-Fi access point for initial setup
- Automatic access point recovery after 30 seconds without a connection
- Captive portal and local web dashboard
- Wi-Fi scanning
- mDNS hostname support
- Persistent settings through ESP32 NVS

### Additional capabilities

- Arduino OTA firmware updates
- OpenWeatherMap current weather
- Three-day weather forecast
- NTP clock synchronization
- Location-based UTC offset
- ESP32 internal chip temperature monitoring

---

## 3. Hardware Requirements

| Component | Quantity |
|---|---:|
| XIAO ESP32-C3 or XIAO ESP32-S3 | 1 |
| SSD1306 128×64 I²C OLED | 1 |
| TTP223 touch sensor | 1 |
| Jumper wires | As needed |
| USB data cable | 1 |

An external speaker, amplifier, or audio module is not required.

### Wiring

| Connection | XIAO ESP32-C3 | XIAO ESP32-S3 |
|---|---|---|
| OLED VCC | 3.3V | 3.3V |
| OLED GND | GND | GND |
| OLED SDA | SDA | SDA |
| OLED SCL | SCL | SCL |
| TTP223 VCC | 3.3V | 3.3V |
| TTP223 GND | GND | GND |
| TTP223 OUT | D1 / GPIO3 | D10 / GPIO9 |

Use a 3.3V-compatible OLED module and touch sensor.

---

## 4. Installing the Firmware

### Step 1 — Install Arduino IDE

Download Arduino IDE 2.x:

https://www.arduino.cc/en/software

### Step 2 — Install ESP32 board support

Open Arduino IDE:

**File → Preferences → Additional Boards Manager URLs**

Add:

```text
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

Open **Boards Manager**, search for `esp32`, and install the Espressif Systems package.

The current firmware is written for the Arduino ESP32 Core 3.x API.

### Step 3 — Install required libraries

Install these libraries using Arduino IDE's Library Manager:

| Library | Purpose |
|---|---|
| U8g2 | OLED graphics and fonts |
| ArduinoJson 7 | JSON parsing and responses |
| NTPClient | Network time synchronization |

Other libraries, including WiFi, WebServer, DNSServer, Preferences, HTTPClient, ESPmDNS, and ArduinoOTA, are provided through the ESP32 Arduino core.

### Step 4 — Open the appropriate source

For the ESP32-C3:

```text
Firmware/
└── Desk-Companion-XIAO-ESP32-C3.ino
```

For the ESP32-S3:

```text
Firmware/
└── Desk-Companion-XIAO-ESP32-S3.ino
```

Open the appropriate `.ino` sketch in Arduino IDE.

### Step 5 — Select the board and port

Select the appropriate XIAO board under **Tools → Board** and select its USB serial port.

Verify that the selected board matches the firmware.

### Step 6 — Configure the flash partition

Desk-Companion contains an embedded web dashboard alongside its firmware features. As a result, the compiled binary may be too large for the default OTA application partition.

Select an **OTA-capable partition scheme** with sufficient application space.

For example, where supported by your selected board:

```text
Minimal SPIFFS (1.9MB APP with OTA)
```

Partition names and availability differ between board definitions.

**Important:**

- The firmware must fit within each OTA application slot.
- A larger single-app partition without OTA support is not suitable for standard Arduino OTA updates.
- Changing the partition layout generally requires a USB flash rather than an OTA update.
- Changing partitions can affect stored configuration data, so be prepared to configure the device again.

### Step 7 — Verify and upload

1. Click **Verify** to compile the sketch.
2. Check the compiled application size.
3. Resolve any reported errors.
4. Click **Upload**.
5. Open Serial Monitor at **115200 baud** to view startup messages.

Do not assume a successful upload until Arduino IDE reports completion.

---

## 5. First-Time Wi-Fi Setup

When Desk-Companion has not completed its initial Wi-Fi configuration, it creates an open access point.

**Network name:**

```text
Desk-Companion-XXXX
```

`XXXX` is generated from the final two bytes of the board's Wi-Fi MAC address, represented as four hexadecimal characters.

Example:

```text
Desk-Companion-57B9
```

This suffix identifies the device. It is not a password.

### Connecting

1. Power on Desk-Companion.
2. Wait for the OLED setup instructions.
3. Connect your phone or computer to the `Desk-Companion-XXXX` network.
4. Open the captive portal if it appears automatically.
5. Otherwise, visit:

```text
http://192.168.4.1
```

6. Scan for available 2.4 GHz Wi-Fi networks.
7. Configure the primary network.
8. Optionally configure a backup network.
9. Save the configuration.

The device completes initial provisioning after successfully connecting to Wi-Fi.

The hotspot then turns off automatically following a short delay.

Saving a network name alone does not complete provisioning; the device must establish a successful connection.

---

## 6. Automatic Wi-Fi Recovery

Desk-Companion is designed to recover when previously saved networks become unavailable.

### Normal operation

When connected to Wi-Fi:

- The access point remains disabled.
- The OLED displays normal screens.
- The web dashboard is available on the local network.
- OTA and internet-dependent features can operate.

### Connection lost

When the connection drops, the firmware attempts to reconnect using its saved Wi-Fi profiles.

The configured retry interval is approximately:

```text
15 seconds
```

### Recovery access point

After approximately **30 seconds continuously disconnected**, a previously provisioned device enables its access point.

```text
Desk-Companion-XXXX
```

The OLED displays recovery instructions.

Connect to the hotspot and open:

```text
http://192.168.4.1
```

You can update the saved Wi-Fi credentials through the dashboard.

### Connection restored

After a successful connection, the recovery access point shuts down following a short delay of approximately 1.8 seconds.

The device then resumes normal operation.

**Note:** The recovery behavior describes the firmware's intended logic. Router compatibility and reconnection reliability still require hardware validation.

---

## 7. Web Dashboard

When Desk-Companion is connected to the same local Wi-Fi as your computer or phone, open:

```text
http://desk-companion.local
```

Alternatively, use the local IP address assigned by your router.

When connected directly to its setup or recovery hotspot, open:

```text
http://192.168.4.1
```

### Dashboard controls

| Feature | Description |
|---|---|
| Overview | Connection status and uptime |
| Screen selection | Change OLED screens remotely |
| Emotion selector | Choose from 66 emotions |
| Automatic emotions | Restore automatic expression switching |
| Wi-Fi settings | Configure primary and backup networks |
| Wi-Fi scan | Discover nearby networks |
| Weather settings | Configure location and API key |
| Restart | Restart the ESP32 |
| Sleep | Put the device into deep sleep |
| Chip temperature | Display the ESP32's internal temperature |

The dashboard is embedded directly within the firmware source using readable HTML, CSS, and JavaScript.

No separate website
