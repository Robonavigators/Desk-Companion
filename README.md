# Desk-Companion

### Your intelligent little desktop companion 🤖

**An open-source ESP32 desktop companion featuring 66 animated emotions, live weather, a smart clock, capacitive touch controls, and a professional web dashboard.**

Designed and developed by **Robonavigators**.

## About

Desk-Companion is an interactive desktop device powered by the Seeed Studio XIAO ESP32-C3 or the XIAO ESP32-S3.

It combines a 128×64 OLED display, expressive facial animations, touch interactions, weather updates, and a built-in web dashboard.

The project is designed to be affordable, customizable, and beginner-friendly.

## Features

- 66 animated facial emotions
- Automatic emotion switching
- Live weather and 3-day forecast
- Location-based clock with NTP synchronization
- Capacitive touch controls
- Deep-sleep functionality
- Professional responsive web dashboard
- SVG icons and custom dialogs
- Searchable emotion controls
- Dual Wi-Fi profiles with automatic reconnection
- Wi-Fi scanning
- Open Wi-Fi access point and captive portal
- Local dashboard at `desk-companion.local`
- ESP32 chip temperature monitoring
- Connection checks every 4 seconds
- NVS persistent configuration
- OTA firmware update support

No external speaker or amplifier is required.

## Supported Boards

| Board | Support |
|---|---|
| Seeed Studio XIAO ESP32-C3 | Dedicated firmware |
| Seeed Studio XIAO ESP32-S3 | Dedicated firmware |

## Hardware Requirements

| Component | Quantity |
|---|---:|
| XIAO ESP32-C3 or ESP32-S3 | 1 |
| SSD1306 128×64 I²C OLED | 1 |
| TTP223 touch sensor | 1 |
| Jumper wires | As required |
| USB data cable | 1 |
| Enclosure | Optional |

## Wiring

### OLED Display

| OLED | XIAO |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SDA | SDA |
| SCL | SCL |

### Touch Sensor

| TTP223 | ESP32-C3 | ESP32-S3 |
|---|---|---|
| VCC | 3.3V | 3.3V |
| GND | GND | GND |
| OUT | D1 / GPIO3 | D10 / GPIO9 |

Always check the board-specific GPIO configuration before connecting components.

---

## Installation

Desk-Companion provides two installation methods depending on your experience level.

| Method | Difficulty | Recommended For |
|---|---|---|
| Web Flash | Beginner | First-time builders |
| Arduino IDE | Intermediate | Developers and advanced users |

## 🚀 Install Firmware via Web Flasher

Install Desk-Companion directly from your desktop browser—**no Arduino IDE installation or manual firmware binary downloads required**. The Robonavigators Web Flasher uses ESP Web Tools to guide you through installation.

**[⚡ Flash Desk-Companion Online](https://robonavigators-io.vercel.app/tools/flash/)**

### Supported firmware builds

| Board | Firmware |
|---|---|
| **XIAO ESP32-C3** | ESP32-C3 firmware |
| **XIAO ESP32-S3** | ESP32-S3 firmware |

ESP Web Tools identifies the connected ESP32 chip family and selects the compatible firmware build from the multi-build JSON manifest. Be sure to connect the correct board and follow the on-screen prompts.

### Installation steps

1. Connect your **Seeed Studio XIAO ESP32-C3** or **XIAO ESP32-S3** to your computer with a **USB data cable**.
2. Open the [Robonavigators Web Flasher](https://robonavigators-io.vercel.app/tools/flash/) in **Google Chrome** or **Microsoft Edge** on a desktop or laptop.
3. Click **Click to Show Firmwares**.
4. Select **Desk-Companion**.
5. Click **Install**.
6. When prompted, select the correct **USB serial device**.
7. Follow the installation prompts until flashing completes. Keep the board connected throughout.
8. Restart the board if necessary, then follow the OLED instructions to configure Wi-Fi.

### Quick troubleshooting

- **Board not detected?** Use a USB **data** cable rather than a charge-only cable, and try another USB port.
- **Connect button unavailable?** Use an up-to-date desktop version of **Chrome** or **Edge** with Web Serial support.
- **Serial port busy?** Close Arduino IDE Serial Monitor, other terminal applications, or anything using the board's port.
- **Flashing won't start?** Reconnect the board and, if needed, enter its **bootloader/download mode** using the board's documented BOOT/RESET procedure before retrying.

For developers who prefer editing the source code, the **Arduino IDE** installation method below remains available. Availability of a particular Web Flash build depends on the published firmware binaries and manifest; source code alone does not guarantee a flashable release.

### Method 1 — Web Flash (Beginner)

**Recommended for users who simply want to build and use Desk-Companion.**

No coding knowledge or Arduino IDE installation is required.

#### Requirements

- XIAO ESP32-C3 or ESP32-S3
- USB data cable
- Desktop or laptop
- Chrome or Microsoft Edge with Web Serial support
- Compatible compiled firmware

#### Installation Steps

1. Open the **Robonavigators Web Flash Tool** at `/flash`.
2. Connect your XIAO ESP32 board using a USB data cable.
3. Select **Desk-Companion**.
4. Choose your board model:
   - XIAO ESP32-C3
   - XIAO ESP32-S3
5. Click **Connect** and select your device's serial port.
6. Start flashing the firmware.
7. Restart the device after installation.
8. Follow the OLED instructions to configure Desk-Companion.

**Note:** Web Flash installation requires the correct compiled `.bin` files and flash manifest to be published. The installation option will be usable once these files are configured in the tool.

### Method 2 — Arduino IDE (Intermediate)

**Recommended for users who want to explore, modify, or develop Desk-Companion's firmware.**

#### Step 1 — Install Arduino IDE

Download Arduino IDE 2.x:

https://www.arduino.cc/en/software

#### Step 2 — Install ESP32 Board Support

Open Arduino IDE and navigate to:

`File → Preferences`

Add this Boards Manager URL:

```text
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

Then open:

`Tools → Board → Boards Manager`

Search for **esp32** and install the Espressif ESP32 board package.

#### Step 3 — Install Libraries

Install the following libraries through Arduino IDE's Library Manager:

- U8g2
- ArduinoJson
- NTPClient

The ESP32 Arduino core provides additional system libraries, including WiFi, WebServer, DNSServer, ESPmDNS, Preferences, ArduinoOTA, and Wire.

#### Step 4 — Download Firmware

Select the appropriate source file:

```text
firmware/
├── XIAO-ESP32-C3/
│   └── Desk-Companion-C3.ino
└── XIAO-ESP32-S3/
    └── Desk-Companion-S3.ino
```

#### Step 5 — Configure the Board

1. Connect the XIAO board with a USB data cable.
2. Select the correct board in Arduino IDE.
3. Select its serial port.
4. Open the corresponding `.ino` file.

#### Step 6 — Compile and Upload

1. Click **Verify** to compile the firmware.
2. Resolve any missing libraries or compilation errors.
3. Click **Upload**.
4. Restart the board after flashing.

For troubleshooting, use Serial Monitor at **115200 baud**.

---

## First-Time Setup

Desk-Companion creates an open Wi-Fi hotspot:

```text
Desk-Companion-XXXX
```

The final four hexadecimal characters are derived from the device's Wi-Fi MAC address.

For example, a MAC address ending in `57:B9` produces:

```text
Desk-Companion-57B9
```

### Configure the Device

1. Connect to the Desk-Companion hotspot.
2. The captive portal may appear automatically.
3. Otherwise, open `http://192.168.4.1`.
4. Scan for nearby Wi-Fi networks.
5. Configure Primary Wi-Fi.
6. Optionally configure Secondary Wi-Fi.
7. Configure location and weather settings.
8. Save your configuration.

Wi-Fi settings are stored in NVS and survive device restarts.

The OLED displays setup instructions when no Wi-Fi profile has been configured. Otherwise, it proceeds to normal operation after the startup screen.

## OLED Startup

The startup screen uses the U8g2 `logisoso16` font:

```text
DESK
COMPANION
```

## Web Dashboard

Once connected to your local network, open:

**http://desk-companion.local**

When connected directly to the device hotspot:

**http://192.168.4.1**

The dashboard provides:

- Device status and uptime
- Wi-Fi signal strength
- ESP32 CPU temperature
- Four-second connection monitoring
- OLED screen switching
- Searchable emotion selector
- Dual Wi-Fi configuration
- Network scanning
- Weather and location configuration
- Restart and sleep controls

The dashboard uses custom SVG icons, dialogs, and notifications.

The CPU temperature represents the ESP32's internal chip temperature, not the ambient room temperature.

mDNS hostname availability depends on the client device and network.

## Weather and Time

Desk-Companion uses OpenWeatherMap for weather information.

Configure:

- City
- State / Province
- Country
- OpenWeatherMap API key

Manual latitude and longitude entry is not required.

Desk-Companion resolves the configured location and retrieves its UTC offset from the weather service.

The clock synchronizes through NTP, using the retrieved offset to display local time.

Get an API key at:

https://openweathermap.org/

An internet connection is required for online weather updates and initial time synchronization.

## OLED Screens

| Screen | Function |
|---|---|
| Emotions | Animated expressions |
| Clock | Local date and time |
| Weather | Current weather |
| Forecast | 3-day forecast |

### Touch Controls

| Action | Result |
|---|---|
| Short touch | Switch screens |
| Long touch | Enter deep sleep |
| Wake touch | Wake where supported |

## Architecture

```text
                 Desk-Companion
                        |
               XIAO ESP32-C3/S3
                        |
       +----------------+----------------+
       |                |                |
    OLED Display    Touch Sensor      Wi-Fi
       |                |                |
    Emotions         Navigation       STA / AP
    Clock            Deep Sleep       Captive Portal
    Weather                           WebServer
    Forecast                          mDNS / OTA
                        |
                    NVS Storage
                        |
                Saved Configuration
                        |
                OpenWeatherMap / NTP
```

## Repository Structure

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
│   ├── installation.md
│   └── architecture.md
├── assets/
│   └── screenshots/
├── README.md
└── LICENSE
```

## Troubleshooting

### Hotspot Not Appearing

Check the USB connection, board power, and installed firmware. Restart the ESP32.

### Captive Portal Not Opening

Connect to the Desk-Companion hotspot and manually open `http://192.168.4.1`.

### desk-companion.local Not Working

Check that your device and Desk-Companion are on the same network. If mDNS is unavailable, use the assigned IP address.

### OLED Not Working

Verify the display's SDA, SCL, VCC, and GND connections and its I²C configuration.

### Weather Not Updating

Check internet connectivity, API key, and configured location.

### Incorrect Clock Time

Verify NTP connectivity and the configured location.

## Security

Desk-Companion currently uses an open setup hotspot and password-free dashboard.

Anyone who can connect to the hotspot or access its web server may be able to control the device or modify settings.

Use the project on trusted networks and do not expose its HTTP or OTA services to the public internet.

Do not commit Wi-Fi passwords, API keys, or other personal information to the repository.

## Development Status

Desk-Companion is under active development.

C3 and S3 firmware versions have been prepared. The latest firmware still requires complete compilation and hardware validation.

Web Flash installation requires compatible binaries and manifests to be published.

## Contributing

Contributions, suggestions, bug reports, and documentation improvements are welcome.

When reporting issues, include the board model, firmware version, relevant logs, and steps to reproduce the problem.

Never include private credentials in issues or screenshots.

## License

**Desk-Companion is licensed under the MIT License.**

You may use, modify, and distribute the project under the terms of the MIT License, including its requirement to retain the copyright and permission notices.

See [LICENSE](LICENSE) for the complete license text.

Third-party dependencies retain their respective licenses.

## Credits

**Desk-Companion — By Robonavigators**

Built using the Seeed Studio XIAO ESP32 platform, Arduino framework, U8g2, and OpenWeatherMap.

---

**Desk-Companion — Your desk. Your companion. Your creativity.**

Made by **Robonavigators**.
