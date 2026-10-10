# 🕒 Simple Word Clock (Swabian Edition)

A modern, embedded German word clock built on **Zephyr RTOS**, featuring a **Swabian dialect front matrix** ("Viertel x", "Halb x", etc.).

Designed for high-precision timekeeping, automatic brightness adjustment, and addressable RGB LED control, making it an ideal DIY wall or desktop clock.

---

## ✨ Features

* **Swabian Dialect Matrix:** Unique layout displaying time in authentic Swabian phrasing.
* **Hardware Flexible (Zephyr RTOS):** Highly portable across various microcontrollers (ESP32, STM32, NXP, etc.) thanks to Zephyr's Devicetree abstraction.
* **Automated Brightness:** Features a **BH1750** ambient light sensor to seamlessly adjust LED brightness according to room lighting.
* **Precise Timekeeping:** High-precision **DS3231 RTC** module with battery backup ensures reliable timekeeping across power interruptions.
* **Addressable LED Matrix:** Driven by **WS2812B** RGB LEDs (60 LEDs/m density).
* **Robust Firmware:** Built on **Zephyr RTOS 4.4.2**.

---

## 🖨️ 3D Printed Parts & Enclosure

The enclosure, front matrix grid, and diffuser plates are optimized for 3D printing.

* 📐 **STL Files:** Download printable files from the [Releases](../../releases) section.
* **Material Recommendation:** **PETG** is highly recommended for mechanical stability and durability.
  * *Tested Setup:* Black PETG (chassis & matrix grid) + transparent PETG (diffuser layer).

---

## 🛠️ Bill of Materials (BOM)

### Electronics & Hardware

| Component | Description | Notes / Selection Rationale |
| :--- | :--- | :--- |
| **Adafruit ItsyBitsy RP2040** | Main microcontroller board running Zephyr RTOS. | **Selected because** it includes an onboard **74HCT125 level shifter**, which is highly recommended for driving the required 5V logic signal of WS2812B LEDs without extra components. |
| **WS2812B LED Strip** | 60 LEDs/m density (~104 LEDs / approx. 2 meters needed). | Required |
| **DS3231 RTC Module** | High-precision I2C real-time clock with coin cell backup. | Required |
| **BH1750 Sensor** | Digital ambient light sensor (I2C interface). | Required |
| **Fuse Holder & Fuse** | Panel-mount inline fuse holder for power safety protection. | Required |
| **DC Jack Panel Mount** | Standard 5.5mm x 2.1mm DC barrel jack for 5V power supply. | Required |
| **Acrylic Front Plate** | Approx. 300 mm diameter clear/smoked acrylic panel. | Optional |
| **Mirrored Window Film** | One-way mirror or window tint film for contrast enhancement. | Optional |

---

## 🔌 Hardware Connections & Pinout

Standard signal wiring as configured in the Zephyr Devicetree:

| Peripheral | Module Pin | Board Pin | Protocol / Notes |
| :--- | :--- | :--- | :--- |
| **DS3231 RTC** | SDA / SCL | I2C0 SDA / SCL | Shared I2C Bus |
| **BH1750 Sensor** | SDA / SCL | I2C0 SDA / SCL | Shared I2C Bus |
| **WS2812B Strip** | Data Input | Pin 5 (5V Logic Out) | Driven directly via the onboard level shifter |
| **Power Input** | 5V / GND | 5V / GND | Powered via DC Jack & fuse |

---

## 💻 Development Environment & Build

This project runs on **Zephyr RTOS 4.4.2**, managed using **West** and **CMake** (T2 Application Repository topology).

### Prerequisites

Follow the official [Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html) to set up host tools (Python, CMake, Ninja, Git, Device Tree Compiler, and the Zephyr SDK toolchain) for Windows, macOS, or Linux.

### Setup & Build Steps

1. **Clone the Application Repository**
   ```bash
   git clone https://github.com/AssFactory/SimpleWordClock.git
   ```

2. **Create & Activate Python Virtual Environment**
   ```bash
   python -m venv .venv
   source .venv/Scripts/activate  # On Linux/macOS: source .venv/bin/activate
   ```

3. **Install West & Initialize Workspace**
   ```bash
   python -m pip install west
   west init -l
   west update
   ```

4. **Install Python Dependencies**
   ```bash
   west packages pip --install
   ```

5. **Build Firmware**
   ```bash
   west build -b adafruit_itsybitsy_rp2040 -p always
   ```

6. **Flash Target**
   Connect the board in bootloader mode (hold the `BOOTSEL` button while plugging in USB)
   ```bash
   west flash
   ```
---

## 🤝 Contributing

Contributions, bug reports, and feature requests are welcome! Feel free to check out the [issues page](../../issues) or submit a pull request.

---

## 📝 License

This project is open-source software licensed under the [MIT License](LICENSE).