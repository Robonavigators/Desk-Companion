# Desk-Companion

**An open-source ESP32 desktop companion with animated emotions, live weather, a smart clock, and a modern web dashboard.**

Developed by **Robonavigators**.

Desk-Companion is a compact, interactive desktop device powered by the Seeed Studio XIAO ESP32 series. It uses a 128×64 OLED display to express emotions, display useful information, and interact through a touch sensor.

Designed to be approachable for beginners while offering advanced features such as a captive portal, local web server, dual Wi-Fi support, and over-the-air firmware updates.

## ✨ Features

- **66 animated emotions** displayed on the OLED
- **Live weather information** using OpenWeatherMap
- **3-day weather forecast**
- **Automatic clock synchronization** with location-based time
- **Capacitive touch controls** for switching screens
- **Deep sleep** for power management
- **Professional web dashboard** with responsive styling and SVG icons
- **Searchable emotion selector**
- **Two saved Wi-Fi profiles** with automatic reconnection
- **Wi-Fi scanning** directly from the dashboard
- **Built-in Wi-Fi access point and captive portal**
- **mDNS support** at `desk-companion.local`
- **ESP32 chip temperature monitoring**
- **Device status monitoring every 4 seconds**
- **Over-the-air firmware updates**
- **Persistent configuration** using ESP32 NVS

No speaker or external audio hardware is required.

## 🔧 Supported Boards

| Board | Support |
|---|---|
| Seeed Studio XIAO ESP32-C3 | Firmware available |
| Seeed Studio XIAO ESP32-S3 | Firmware available |

Use the firmware specifically designed for your board.

**Note:** The current firmware builds require compilation and hardware validation before they can be considered fully tested releases.

## 📦 Hardware Requirements

| Component | Quantity |
|---|---|
| XIAO ESP32-C3 or XIAO ESP32-S3 | 1 |
| SSD1306 128×64 I²C OLED display | 1 |
| TTP223 capacitive touch sensor | 1 |
| Jumper wires | As required |
| USB data cable | 1 |
| Suitable enclosure | Optional |

## 🔌 Wiring

### OLED Display — Both Boards

| OLED Pin | XIAO Connection |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SDA | SDA |
| SCL | SCL |

### Touch Sensor

| TTP223 Pin | XIAO ESP32-C3 | XIAO ESP32-S3 |
|---|---|---|
| VCC | 3.3V | 3.3V |
| GND | GND | GND |
| OUT | D1 / GPIO3 | D10 / GPIO9 |

The touch sensor uses an active-HIGH output configuration.

**Important:** Check the GPIO definitions in the firmware version you are uploading. Use 3.3V-compatible signals.

## 💻 Software Requirements

- Arduino IDE 2.x
- Espressif ESP32 board package
- Arduino-compatible USB drivers, if needed

### Libraries

Install the libraries required by the selected firmware, including:

- U8g2
- ArduinoJson
- NTPClient

The ESP32 Arduino core provides libraries such as `WiFi.h`, `WebServer.h`, `DNSServer.h`, `Preferences.h`, and `ArduinoOTA.h`.

## 🚀 Installation

1. Install Arduino IDE and the ESP32 board package.
2. Install the required libraries.
3. Download the firmware for your board.
4. Connect your XIAO board using a USB data cable.
5. Select the correct XIAO ESP32-C3 or ESP32-S3 board in Arduino IDE.
6. Select the appropriate serial port.
7. Compile and upload the firmware.
8. Open Serial Monitor at 115200 baud if debugging is required.

Do not upload the C3 firmware to an S3 board or vice versa.

## 📡 First-Time Setup

When Desk-Companion boots, it displays its startup animation.

If no Wi-Fi profile is saved in NVS, the OLED displays instructions for first-time setup.

Desk-Companion creates a Wi-Fi network named:

```text
Desk-Companion-XXXX
```

The final four hexadecimal characters are derived from the device's Wi-Fi MAC address.

For example:

```text
Desk-Companion-57B9
```

### Connect and Configure

1. Connect your phone or computer to the Desk-Companion Wi-Fi network.
2. The captive portal may open automatically.
3. Otherwise, visit `http://192.168.4.1`.
4. Configure your Wi-Fi credentials.
5. Enter your weather API key and location if you want weather and automatic local-time features.
6. Save your settings.

The access point is open and does not require a password.

Once Wi-Fi configuration is saved in NVS, Desk-Companion skips the first-time setup screen on subsequent boots.

## 🌐 Web Dashboard

After connecting Desk-Companion to your local Wi-Fi, open:

```text
http://desk-companion.local
```

Alternatively, use the device's assigned IP address.

When connected directly to the Desk-Companion access point, use:

```text
http://192.168.4.1
```

### Dashboard Capabilities

- View connection status, uptime, and chip temperature
- Monitor device response times
- Switch between OLED screens
- Search and select emotions
- Configure primary and secondary Wi-Fi networks
- Scan for nearby Wi-Fi networks
- Configure weather and location
- Restart the device
- Access sleep controls

The interface uses custom dialogs and notifications instead of default browser pop-ups.

**Note:** mDNS support depends on the client device and network configuration.

## 🌤️ Weather and Local Time

Desk-Companion uses OpenWeatherMap for weather information and location-based time configuration.

Users can enter:

- City
- State / Province
- Country
- OpenWeatherMap API key

Latitude and longitude do not need to be entered manually.

The firmware resolves the configured location through OpenWeatherMap and obtains its UTC offset. Time synchronization is performed using NTP.

An internet connection is required for initial synchronization and online weather updates.

The temperature shown in the dashboard's device overview represents the **ESP32 chip temperature**, not the outdoor or room temperature.

## 👆 Touch Controls

| Action | Function |
|---|---|
| Short touch | Switch OLED screen |
| Long touch | Enter deep sleep |
| Touch after supported wake-up | Wake the device |

Available screens include emotions, clock, current weather, and forecast.

## 🔐 Security

Desk-Companion is designed for local-network operation.

The current configuration uses an open Wi-Fi access point and a dashboard without authentication.

**Anyone connected to the access point or an accessible local network may be able to control the device or modify its settings.**

For safer operation:

- Use trusted networks.
- Avoid exposing the web server directly to the internet.
- Never hardcode personal Wi-Fi credentials or weather API keys in public source code.
- Keep secrets in local persistent configuration.
- Review firmware update security before deploying to shared or untrusted networks.

## 📁 Suggested Repository Structure

```text
Desk-Companion/
├── firmware/
│   ├── XIAO-ESP32-C3/
│   │   └── Desk-Companion-C3.ino
│   └── XIAO-ESP32-S3/
│       └── Desk-Companion-S3.ino
├── dashboard/
│   └── index.html
├── docs/
│   ├── wiring.md
│   ├── setup.md
│   └── architecture.md
├── assets/
│   └── screenshots/
├── README.md
└── LICENSE
```

## 🏗️ Architecture Overview

```text
                Desk-Companion
                       |
              XIAO ESP32-C3/S3
                       |
        +--------------+--------------+
        |              |              |
     OLED Display   Touch Sensor   Wi-Fi
        |              |              |
      Emotions      Navigation     WebServer
      Clock         Deep Sleep     Captive Portal
      Weather                      Dashboard
                                   OTA
                       |
                  NVS Storage
                       |
              Device Configuration
```

## 🤝 Contributing

Contributions, bug reports, documentation improvements, and feature suggestions are welcome.

When reporting an issue, include your board model, Arduino ESP32 core version, relevant error messages, and steps to reproduce the problem.

Please avoid sharing Wi-Fi passwords, API keys, or other private information in issues and screenshots.

## 📄 License

A license has not yet been selected.

Before publishing the project as open source, add a `LICENSE` file specifying the terms under which others may use, modify, and redistribute it.

The MIT License is one possible choice.

## 👨‍💻 Credits

**Desk-Companion**

Designed and developed by **Robonavigators**.

Built using the ESP32 platform, Arduino ecosystem, U8g2 graphics library, and OpenWeatherMap services.

---

**Made with curiosity, creativity, and engineering.**

*Desk-Companion — a little personality for your desk.*
