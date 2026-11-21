# Desk-Companion

### Your intelligent little desktop companion. 🤖

**An open-source, ESP32-powered desktop companion featuring 66 animated emotions, live weather, a smart clock, capacitive touch controls, and a professional web dashboard.**

Developed by **Robonavigators**.

---

## 🌟 About the Project

Desk-Companion is a compact, interactive desktop device built around the **Seeed Studio XIAO ESP32-C3 and ESP32-S3** microcontrollers.

It features a 128×64 OLED display that can express 66 different emotions, display real-time information, and interact through a capacitive touch sensor.

Desk-Companion also provides a built-in web dashboard, allowing users to configure Wi-Fi, manage weather settings, monitor the device, and control its display directly from a browser.

The project is designed to be affordable, beginner-friendly, and easy to reproduce.

**No speaker, mobile application, or dedicated cloud account is required to operate the local dashboard.**

## ✨ Features

### 🤖 Interactive OLED Companion
- 66 animated facial emotions
- Automatic emotion switching
- Expressive eyes and facial animations
- Smooth pupil movement
- Capacitive touch navigation
- Deep-sleep support

### 🌤️ Weather and Time
- Live temperature and weather conditions
- Humidity and wind information
- Three-day weather forecast
- NTP clock synchronization
- Automatic local time based on configured location
- Custom mist and fog graphics
- Location configuration using City, State, and Country

Weather information is provided through OpenWeatherMap.

### 🌐 Web Dashboard
- Professional responsive interface
- Custom SVG icons
- Desktop and mobile layouts
- Live connection monitoring
- Four-second device status checks
- ESP32 chip temperature monitoring
- OLED screen controls
- Searchable emotion selector
- Custom confirmation dialogs
- Non-intrusive toast notifications
- Wi-Fi scanning
- Device restart and sleep controls

### 📡 Connectivity
- Two configurable Wi-Fi profiles
- Automatic reconnection and fallback
- Built-in Wi-Fi access point
- Captive portal for first-time configuration
- mDNS hostname
- Over-the-air firmware update support
- Persistent settings using ESP32 NVS

---

## 🛠️ Supported Hardware

| Board | Firmware |
|---|---|
| Seeed Studio XIAO ESP32-C3 | Dedicated C3 edition |
| Seeed Studio XIAO ESP32-S3 | Dedicated S3 edition |

Choose the firmware specifically designed for your board.

The two editions share the same main features but have different hardware configurations and sleep implementations.

### Components Required

| Component | Quantity |
|---|---:|
| XIAO ESP32-C3 or ESP32-S3 | 1 |
| SSD1306 128×64 I²C OLED | 1 |
| TTP223 capacitive touch sensor | 1 |
| Jumper wires | As required |
| USB data cable | 1 |
| Enclosure | Optional |

An external speaker or audio amplifier is not required.

---

## 🔌 Wiring Guide

### OLED — Both Editions

| SSD1306 OLED | XIAO ESP32 |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SDA | SDA |
| SCL | SCL |

Use the SDA and SCL pins labeled on the XIAO board.

### TTP223 Touch Sensor

| Touch Sensor | XIAO ESP32-C3 | XIAO ESP32-S3 |
|---|---|---|
| VCC | 3.3V | 3.3V |
| GND | GND | GND |
| OUT | D1 / GPIO3 | D10 / GPIO9 |

The firmware expects an active-HIGH touch output.

**Important:** Verify the board-specific pin definitions before uploading the firmware. Do not apply 5V directly to ESP32 GPIO pins.

---

## 🚀 Installation

Desk-Companion supports two installation methods.

### Method 1 — Web Flash (Recommended)

The easiest way to install Desk-Companion is through the **Robonavigators Web Flash Tool (`/flash`)**.

This method is intended for beginners and users who do not want to install Arduino IDE.

#### Requirements

- A supported XIAO ESP32-C3 or ESP32-S3
- A USB data cable
- A computer running a compatible browser
- A compatible, precompiled firmware image

Chrome or Microsoft Edge on a supported desktop operating system is recommended for Web Serial flashing.

#### Installation Steps

1. Open the Robonavigators Web Flash Tool.
2. Connect your XIAO ESP32 board using a USB data cable.
3. Select **Desk-Companion**.
4. Choose the appropriate board:
   - XIAO ESP32-C3
   - XIAO ESP32-S3
5. Click **Connect**.
6. Select the board's serial port when prompted.
7. Start the firmware flashing process.
8. Allow the board to restart after installation.

Once flashing is complete, Desk-Companion will display its startup screen.

**Important:** Web flashing requires compiled firmware images such as `.bin` files and the correct flash configuration. Arduino `.ino` files cannot be flashed directly through a standard browser-based ESP32 flasher.

The Web Flash installation flow requires compatible firmware binaries and a configured flasher manifest to be published.

### Method 2 — Arduino IDE

This method is recommended for developers who want to inspect, customize, or modify Desk-Companion's firmware.

#### Step 1 — Install Arduino IDE

Download and install Arduino IDE 2.x from the official Arduino website.

https://www.arduino.cc/en/software

#### Step 2 — Install ESP32 Board Support

Open:

`File → Preferences`

Add the Espressif ESP32 boards manager URL:

```text
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

Then open:

`Tools → Board → Boards Manager`

Search for **esp32** and install the Espressif ESP32 board package.

#### Step 3 — Install Required Libraries

The firmware uses:

- U8g2
- ArduinoJson
- NTPClient

The ESP32 Arduino core also provides the required networking and system libraries, including:

- WiFi
- WebServer
- DNSServer
- ESPmDNS
- Preferences
- ArduinoOTA
- Wire

#### Step 4 — Download Firmware

Choose the corresponding firmware from the repository:

```text
firmware/
├── XIAO-ESP32-C3/
│   └── Desk-Companion-C3.ino
└── XIAO-ESP32-S3/
    └── Desk-Companion-S3.ino
```

#### Step 5 — Configure Arduino IDE

Connect your board to the computer.

Select the correct board under:

`Tools → Board`

Then select the appropriate USB port.

#### Step 6 — Upload

Open the firmware, click **Verify**, and then **Upload**.

Once the upload is complete, restart Desk-Companion.

For debugging, open Serial Monitor at:

```text
