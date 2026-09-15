# 🕒 Simple word clock

This repository contains the complete hardware design (3D-printed enclosure), electronics schematics, and the PlatformIO-based firmware.

---

## 🚀 Features
*   **Swabian Dialect Matrix:** Unique front layout displaying time like a true Swabian.
*   **Automated Brightness:** Uses a BH1750 ambient light sensor to dim the LEDs at night.
*   **Ultra-Precise Time:** Keeps exact time via a DS3231 RTC module, backed up by a DCF77 radio clock receiver for automatic synchronization.
*   **Audio Announcements:** Plays hours, chimes, or alarms using a DFPlayer Mini.
*   **Stable Signals:** Features a 74HCT125 level shifter to boost the ESP32's 3.3V logic level to the 5V required by the WS2812B LEDs.

---

## 🛠️ Hardware & Components (BOM)

| Component | Description |
| :--- | :--- |
| **ESP32 S2 Super Mini** | The compact main microcontroller driving the clock. |
| **WS2812B LED Strip** | Individual addressable RGB LEDs for the matrix illumination. |
| **DS3231 RTC** | High-precision Real-Time Clock with battery backup. |
| **DCF77 Module** | Atomic clock radio receiver for automated time setting. |
| **BH1750 Sensor** | Digital light sensor for adaptive brightness. |
| **DFPlayer Mini** | MP3/WAV audio module for sound output + MicroSD slot. |
| **74HCT125** | Quad bus buffer used as a 3.3V to 5V logic level shifter. |

---

## 💻 Firmware Installation

This project is built using **PlatformIO** inside VS Code.

1. Clone this repository: `git clone https://github.com`
2. Open the `firmware/` folder inside VS Code.
3. PlatformIO will automatically fetch all needed library dependencies.
4. Connect your **ESP32 S2 Super Mini** via USB.
5. Click the **PlatformIO: Upload** button (arrow icon in the bottom status bar).

---

## 📝 License
This project is licensed under the MIT License - see the `LICENSE` file for details.