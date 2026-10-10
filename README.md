# 🕒 Simple Word Clock (Swabian Edition)

A custom, embedded German word clock featuring a **Swabian dialect matrix**, powered by [Zephyr RTOS](https://www.zephyrproject.org/).

Designed around the **Adafruit ItsyBitsy RP2040**, this open-source project combines precise timekeeping, automatic brightness control, and addressable RGB LEDs into an elegant wall or desktop clock.

---

## 🚀 Features

* **Swabian Dialect Matrix:** Unique front layout displaying time in a Swabian-inspired word arrangement ("Viertel x", "Halb x", etc.).
* **Integrated Level Shifter:** Built around the **Adafruit ItsyBitsy RP2040**, which features an onboard **74HCT125 level shifter**. This simplifies wiring significantly by driving the 5V logic signal for WS2812B LEDs directly.
* **Hardware Flexible (Zephyr RTOS):** Thanks to Zephyr's Devicetree abstraction, the firmware is highly portable and can easily be ported to other microcontrollers (e.g., **ESP32, STM32, NXP**).
* **Automated Brightness:** Uses a **BH1750** ambient light sensor to seamlessly adjust LED brightness based on surrounding ambient light.
* **Precise Timekeeping:** High-precision **DS3231 RTC** with battery backup ensures accurate timekeeping across power loss.
* **Addressable LED Matrix:** Driven by **WS2812B** individually addressable RGB LEDs (60 LEDs/m density).
* **Robust Firmware:** Built on **Zephyr RTOS 4.4.2**.

---

## 🖨️ 3D Printed Parts & Enclosure

The housing, front matrix grid, and diffuser plates are designed for 3D printing.

* 📐 **3D Models & STL Files:** Download the printable files from [Printables / Thingiverse / GitHub Release Placeholders](https://example.com/your-3d-clock-files).
* **Material Recommendation:** **PETG** is highly recommended for stability and durability. 
  * *Tested Setup:* Black PETG (for the chassis/grid) and transparent PETG (for the diffusers).
  * *Alternatives:* White PETG or other materials (like PLA) should also work fine depending on your printer setup.

---

## 🛠️ Bill of Materials (BOM)

### Electronics & Hardware

| Component                     | Description                                                                      | Notes    |
| :---                          | :---                                                                             | :---     |
| **Adafruit ItsyBitsy RP2040** | Main microcontroller running Zephyr RTOS (includes onboard 74HCT level shifter). | Required |
| **WS2812B LED Strip**         | 60 LEDs/m density (required 104 LEDs ~approx. 2m).                               | Required |
| **DS3231 RTC Module**         | High-precision I2C real-time clock with coin cell backup battery.                | Required |
| **BH1750 Module**             | Digital ambient light sensor (I2C) for automatic brightness adjustment.          | Required |
| **Fuse Panel Mount**          | Inline/Panel-mount fuse holder for power safety protection.                      | Required |
| **DC Jack Panel Mount**       | Standard 5.5mm x 2.1mm DC barrel connector for 5V power input.                   | Required |
| **Plexiglass / Acrylic Disc** | 300 mm diameter acrylic panel for a clean front finish.                          | Optional |
| **Mirrored Window Tint Film** | One-way mirror, car window tint, etc ...                                         | Optional |
---

## 🔌 Hardware Connections & Pinout

Below is the standard wiring assignment configured in the Zephyr Devicetree:

| Peripherals | Module Pin | Board Pin (ItsyBitsy RP2040) | Protocol / Notes |
| :--- | :--- | :--- | :--- |
| **DS3231 RTC** | SDA / SCL | I2C0 SDA / SCL | I2C Bus |
| **BH1750 Sensor** | SDA / SCL | I2C0 SDA / SCL | Shared I2C Bus |
| **WS2812B Strip** | Data Input | Pin 5 (5V Logic Out) | Driven via onboard 74HCT level shifter |
| **Power Input** | 5V / GND | 5V / GND | Powered via DC Jack + Fuse |

---

## 💻 Development Environment & Build

This project uses **Zephyr RTOS 4.4.2**, managed via the **West** meta-tool and **CMake**. It follows Zephyr's **T2 (Application Repository) workspace topology**.

### Prerequisites

Follow the official [Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html) to set up host tools (Python, CMake, Ninja, Git, Device Tree Compiler, and the Zephyr SDK toolchain) for Windows, macOS, or Linux.

### Setup & Build Steps

1. **Clone the Application Repository**
   ```bash
   git clone https://github.com/AssFactory/SimpleWordClock.git
   cd SimpleWordClock
   ```

2. **Create & Activate Python Virtual Environment**
   ```bash
   cd ..
   python -m venv .venv
   source .venv/Scripts/activate  # On Linux/macOS use: source .venv/bin/activate
   ```

3. **Install West Meta-Tool**
   ```bash
   python -m pip install west
   ```

4. **Initialize Workspace & Fetch Dependencies**
   ```bash
   cd SimpleWordClock
   west init -l
   west update
   ```

5. **Install Python Package Dependencies**
   ```bash
   west packages pip --install
   ```

6. **Build Firmware**
   ```bash
   west build -b adafruit_itsybitsy_rp2040 -p always
   ```

7. **Flash to Target**
   Connect the Adafruit ItsyBitsy RP2040 in bootloader mode (hold `BOOTSEL` button while plugging in USB) and flash:
   ```bash
   west flash
   ```

---

## 🤝 Contributing

Contributions, bug reports, and feature requests are welcome! Feel free to check out the [issues page](../../issues) or submit a pull request.

---

## 📝 License

This project is open-source software licensed under the [MIT License](LICENSE).