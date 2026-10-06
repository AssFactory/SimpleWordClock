# 🕒 Simple Word Clock

A German word clock project based on the **Adafruit ItsyBitsy RP2040**.

The project combines a word-based LED matrix with a real-time clock and ambient light sensing. The firmware is developed using **Zephyr RTOS**.

The project uses Zephyr's **T2 (Application Repository) workspace topology**. The application repository contains the project-specific files and `west.yml`, while Zephyr and its required modules are managed separately by West in the workspace.

---

## 🚀 Features

* **Swabian Dialect Matrix:** Unique front layout displaying the time in a Swabian-inspired word arrangement.
* **Automated Brightness:** Uses a **BH1750** ambient light sensor to adjust the LED brightness according to the surrounding light.
* **Precise Timekeeping:** Uses a **DS3231 RTC** for accurate timekeeping with battery backup.
* **Addressable LED Matrix:** Controls the clock illumination using **WS2812B** individually addressable RGB LEDs.

---

## 🛠️ Hardware & Components

| Component                     | Description                                                       |
| :---------------------------- | :---------------------------------------------------------------- |
| **Adafruit ItsyBitsy RP2040** | Main microcontroller running the Zephyr firmware.                 |
| **WS2812B LED Matrix**        | Individually addressable RGB LEDs for displaying the time.        |
| **DS3231 RTC**                | High-precision real-time clock with battery backup.               |
| **BH1750 Sensor**             | Digital ambient light sensor for automatic brightness adjustment. |

---

## 💻 Development Environment & Build

The firmware is based on **Zephyr RTOS 4.4.0** and uses **West** and **CMake**.

The project follows Zephyr's **T2 (Application Repository) workspace topology**. The application repository acts as the local West manifest repository, while the Zephyr source tree and required modules are maintained by West in the workspace.

### Prerequisites

Before setting up this project, install the required host tools according to the official Zephyr **Getting Started Guide**:

[Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html?utm_source=chatgpt.com)

The guide covers the required tools for Windows, Linux, and macOS, including Python, CMake, Ninja, Git, the Device Tree Compiler, and the Zephyr SDK/toolchain.


### 1. Clone the repository

Clone the application repository

```bash
git clone
```

### 2. Create the Python virtual environment

The Python virtual environment is kept outside the Git repository.

```bash
cd ..
python -m venv .venv
```

Activate environment

```bash
source .venv/Scripts/activate
```

### 3. Install West

Install West into the active virtual environment.

```bash
python -m pip install west
```

### 4. Initialize the Zephyr workspace

Change to the application repository containing `west.yml` and nitialize the West workspace using the **local** manifest.

```bash
cd /SimpleWordClock
west init -l .
```

Fetch Zephyr and the modules defined by `west.yml`

```bash
west update
```

### 5. Install Zephyr Python dependencies

Install all Python packages required by the checked-out Zephyr version and its configured modules.

```bash
west packages pip --install
```
### 6. Build the firmware

Build the application.

```bash
west build -b adafruit_itsybitsy_rp2040 -p always
```
## 📝 License

This project is licensed under the MIT License - see the `LICENSE` file for details.
