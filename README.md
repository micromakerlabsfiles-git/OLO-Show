# 🎬 OLO Show — Browser Flasher & Custom Animation Toolchain

> Convert any video or GIF into a 1-bit OLED animation and flash it wirelessly to your ESP32 device — all from your browser.

---

## 📑 Table of Contents

1. [Project Overview](#-project-overview)
2. [Required Files & GitHub Structure](#-required-files--github-structure)
3. [Hardware Requirements](#%EF%B8%8F-hardware-requirements)
4. [Software Requirements](#-software-requirements)
5. [Wiring Guide](#-wiring-guide)
6. [User Manual](#-user-manual)
   - [Step 1 — Convert Your Videos](#step-1--convert-your-videos)
   - [Step 2 — Build Custom Firmware](#step-2--build-custom-firmware)
   - [Step 3 — Flash to Device](#step-3--flash-to-device)
   - [Step 4 — Real-Time USB Controller](#step-4--real-time-usb-controller)
7. [Touch Controls & Gestures](#-touch-controls--gestures)
8. [Configuration Reference](#%EF%B8%8F-configuration-reference)
9. [Building from Source](#%EF%B8%8F-building-from-source)
10. [Troubleshooting](#-troubleshooting)

---

## 🧩 Project Overview

OLO Show is a complete end-to-end toolchain for loading custom 1-bit animations onto a microcontroller-driven OLED display:

| Component | Description |
|---|---|
| `index.html` | All-in-one browser tool: animation converter, browser flasher, real-time USB controller |
| `omggif.js` | GIF decoder library used by `index.html` — must be kept in the same folder |
| `build_firmware.py` | GUI application that compiles `.h` animation headers into a flashable `.bin` via PlatformIO |
| `OLO_Show_Builder.exe` | Pre-packaged Windows executable of `build_firmware.py` (no Python needed) |
| `src/main.cpp` | ESP32/RP2040 Arduino firmware with animation playback, clock, LED effects, and WebSerial control |
| `include/Config.h` | Hardware pin assignments and feature flags |
| `include/GeneratedVideos.h` | Auto-generated animation registry that maps slots to video header arrays |
| `platformio.ini` | Multi-environment build config (ESP32 Dev, ESP32-C3, RP2040 Pico W) |

---

## 📦 Required Files & GitHub Structure

### ✅ Files to Upload to GitHub

```
OLO_Show/
│
├── 📄 README.md                    ← Project documentation (this file)
├── 📄 .gitignore                   ← Git ignore rules
├── 📄 platformio.ini               ← PlatformIO build configuration
├── 📄 build_firmware.py            ← GUI firmware builder script (Python/Tkinter)
├── 📄 build_exe.bat                ← Windows batch to package builder as .exe
├── 📄 OLO_Show_Builder.spec        ← PyInstaller spec file for .exe packaging
├── 📄 index.html                   ← All-in-one browser tool (converter + flasher + controller)
├── 📄 omggif.js                    ← GIF decoder library (required by index.html)
│
├── 📁 src/
│   └── 📄 main.cpp                 ← Main ESP32/RP2040 firmware source code
│
└── 📁 include/
    ├── 📄 Config.h                 ← Hardware pin definitions & feature configuration
    └── 📄 GeneratedVideos.h        ← Animation slot registry (example / auto-generated)
```

> **Note on `include/video1.h`:** This is a large sample animation header (~1.2 MB). It is optional — include it as a demo or leave it out and have users generate their own using `index.html`.

---

### ❌ Files to Exclude from GitHub

These files are auto-generated, too large for Git, or machine-specific. **Do not commit them:**

| File / Folder | Reason to Exclude |
|---|---|
| `.pio/` | PlatformIO build cache (auto-generated on every build) |
| `build/` | PyInstaller build artifacts |
| `dist/` | Compiled `.exe` output from PyInstaller |
| `firmware.bin` | Pre-built factory binary → host via **GitHub Releases** instead |
| `firmware_custom.bin` | User-generated custom binary (created locally per-user) |
| `OLO_Show_Builder.exe` | Compiled Windows executable → host via **GitHub Releases** |
| `OLO_Show.7z` | Project archive → host via **GitHub Releases** |
| `*.mp4` / `*.gif` / `*.avi` | Sample media files (large; user supplies their own) |
| `include/video*.h` | Large auto-generated animation headers (~1 MB+ each) |
| `.vscode/` | IDE-specific settings (partially covered by `.gitignore`) |

### 📋 Recommended `.gitignore`

```gitignore
# PlatformIO build cache
.pio/

# PyInstaller build output
build/
dist/

# Compiled binaries — release these via GitHub Releases
*.bin
*.exe
*.7z

# Large auto-generated animation headers
include/video*.h

# Sample media files
*.mp4
*.gif
*.avi
*.mov

# IDE settings
.vscode/.browse.c_cpp.db*
.vscode/c_cpp_properties.json
.vscode/launch.json
.vscode/ipch
```

> **Tip:** Upload `firmware.bin`, `OLO_Show_Builder.exe`, and `OLO_Show.7z` as **GitHub Release assets** so users can download them without cloning the full repository.

---

## 🛠️ Hardware Requirements

| Component | Specification |
|---|---|
| **Microcontroller** | ESP32-C3 Supermini *(primary)*, ESP32 DevKit, or Raspberry Pi Pico W |
| **OLED Display** | 128×64 SH1106 **or** SSD1306 (I2C interface) |
| **Capacitive Touch Sensor** | TTP223 or equivalent (active HIGH output) |
| **Buzzer** | 3.3 V passive or active buzzer |
| **RGB LED** | WS2812B NeoPixel (single onboard pixel or strip) |
| **USB Cable** | USB-C or Micro-USB with **data lines** (not charge-only) |
| **Browser** | Google Chrome or Microsoft Edge (WebSerial API required) |

---

## 💻 Software Requirements

### For Browser Flashing & Animation Conversion
- **Google Chrome** or **Microsoft Edge** (latest version)
- No installation needed — just open `index.html` locally

### For Building Custom Firmware

**Option A — Pre-built EXE (Windows, no Python required):**
- Download `OLO_Show_Builder.exe` from [Releases](../../releases)
- Place it in the same folder as `platformio.ini`

**Option B — Python Script:**
- **Python 3.8+** — [python.org](https://python.org)
- **PlatformIO Core CLI** — [platformio.org/install/cli](https://platformio.org/install/cli)
- **Pillow** *(optional, for the builder logo)*: `pip install pillow`
- Tkinter ships with most Python installs (no extra install needed)

**Option C — PlatformIO IDE (VS Code):**
- VS Code + PlatformIO extension
- Open the project folder and build the desired environment

---

## 🔌 Wiring Guide

All pins are configurable in [`include/Config.h`](include/Config.h).

| Device | Signal | ESP32-C3 GPIO | Notes |
|---|---|---|---|
| OLED Display | SDA | **GPIO 20** | I2C Data |
| OLED Display | SCL | **GPIO 21** | I2C Clock |
| OLED Display | VCC | 3.3 V | |
| OLED Display | GND | GND | |
| Capacitive Touch | OUT | **GPIO 1** | Active HIGH |
| Capacitive Touch | VCC | 3.3 V | |
| Capacitive Touch | GND | GND | |
| Buzzer | + | **GPIO 2** | PWM tone output |
| Buzzer | − | GND | |
| NeoPixel RGB LED | DIN | **GPIO 6** | WS2812B data line |
| NeoPixel RGB LED | VCC | 3.3 V – 5 V | |
| NeoPixel RGB LED | GND | GND | |

> **Standard ESP32 DevKit / Pico W:** Same GPIO numbers apply. Remap in `Config.h` if your board has conflicts.

---

## 📖 User Manual

### Step 1 — Convert Your Videos

> **Tool:** `index.html` → **Animation Converter** tab  
> **Browser required:** Google Chrome or Microsoft Edge

1. **Open `index.html`** by double-clicking it or dragging it into Chrome/Edge.
2. Click the **Animation Converter** tab.
3. Load video files into slots:
   - **Single upload:** Click the 📁 folder icon next to any slot and pick a `.mp4`, `.gif`, or `.avi` file.
   - **Add more slots:** Click **➕ Add Slot** (up to 10 slots supported).
   - **Bulk import:** Click **📦 Load ZIP of Videos** — the tool unpacks and fills all slots automatically.
4. Tune the conversion settings:

   | Setting | Description |
   |---|---|
   | **Dithering** | Black-and-white rendering algorithm (Floyd-Steinberg, Threshold, Atkinson, etc.) |
   | **Scaling** | How video fits the 128×64 canvas (`Fit`, `Fill`, `Stretch`) |
   | **FPS** | Target playback frame rate (lower = smaller file, higher = smoother animation) |
   | **Invert** | Swap black and white pixels |

5. Click **🚀 Convert & Process All Loaded Videos** and wait for all slots to finish.
6. Download the converted files:
   - **Single slot:** Click **📥 Download Preview Slot (.h)** for the currently previewed animation.
   - **All slots:** Click **📦 Download All as ZIP** → saves `olo_show_headers.zip` containing:
     - `video1.h`, `video2.h`, … — individual animation frame data arrays
     - `GeneratedVideos.h` — the slot registry linking all headers

---

### Step 2 — Build Custom Firmware

> **Tool:** `OLO_Show_Builder.exe` or `build_firmware.py`  
> **Requires:** PlatformIO Core CLI installed (only for the Python script route)

#### Launch the Builder

- **Windows EXE (recommended):** Double-click `OLO_Show_Builder.exe` placed in the project root.
- **Python script:** Run `python build_firmware.py` from the project root folder.

#### Inside the Builder GUI

1. **Load animation headers** — choose one method:
   - **➕ Add Video Source (.h):** Browse and pick individual `.h` files.
   - **📦 Load ZIP of Headers:** Select `olo_show_headers.zip` from Step 1 — files are unpacked and mapped automatically.
   - **🧹 Clear All Paths:** Reset file inputs without removing slot rows.
   - **🗑️ Remove All Slots:** Fully reset the slot list.

2. **Select your OLED display driver:**
   - `SH1106` — most 1.3" OLED displays
   - `SSD1306` — most 0.96" OLED displays

3. **Select your target board:**
   - `ESP32-C3 Supermini` *(recommended)*
   - `ESP32 DevKit`
   - `RP2040 Pico W`

4. Click **🚀 BUILD CUSTOM FIRMWARE BINARY**.

5. The builder will:
   - Copy `.h` files into `include/`
   - Write a fresh `GeneratedVideos.h` registry
   - Run PlatformIO to compile the firmware
   - Verify the binary is **under 3.0 MB**

6. On success, **`firmware_custom.bin`** is saved in the project root.

> ⚠️ **3 MB size limit:** The ESP32 app partition is limited to ~3 MB. If the build exceeds this, reduce the number of animation slots, lower FPS, or trim video length.

---

### Step 3 — Flash to Device

> **Tool:** `index.html` → **Standard Factory Flasher** or **Custom Binary Flasher** tab  
> **Browser required:** Chrome or Edge (WebSerial needs HTTPS or `localhost`)

#### Option A: Flash Factory Firmware (Reset to Default)

1. Open `index.html` → **Standard Factory Flasher** tab.
2. Connect your device via USB.
3. Click **Install**, select your COM port, and follow on-screen instructions.
4. The default `firmware.bin` will be flashed automatically.

#### Option B: Flash Your Custom Firmware

1. Open `index.html` → **Custom Binary Flasher** tab.
2. Click **Choose File** and select your `firmware_custom.bin`.
3. Connect your device via USB.
4. Click **🔌 Connect Device** → choose the correct COM port.
5. Click **⚡ Flash Binary** and wait for the progress bar to complete.
6. Click **Reset Device** (or press the physical RESET button).
7. Your custom animations start playing immediately! 🎉

> **COM port not appearing?** Use a data-capable USB cable. Install **CP210x** or **CH340** USB drivers for your board.

---

### Step 4 — Real-Time USB Controller

> **Tool:** `index.html` → **USB Web Controller** tab  
> **Requires:** Device flashed and connected via USB

1. Open `index.html` → **USB Web Controller** tab.
2. Click **Connect** and select your device's COM port.
3. Control your device live:

| Control | Description |
|---|---|
| ▶ **Play** | Resume animation playback |
| ⏸ **Pause** | Freeze on current frame |
| **LED Pattern** | 11 built-in effects: OFF, Rainbow, Solid colors, Fire, Sparkle, Chase, Breath |
| **Play Mode** | `🔁 Single File Loop` / `➡️ Play One-by-One` / `🔀 Random` |
| **Gap Delay** | Black-screen pause time between animations (ms) |
| **Set Clock** | Sync the device's onboard clock |

> **Safe to disconnect at any time** — the WebSerial reader terminates cleanly with no browser freezes.

---

## 👆 Touch Controls & Gestures

| Gesture | Action |
|---|---|
| **Single Tap** | Play / Pause animation |
| **Double Tap** | Advance to the next slot (cycles all non-empty slots, then shows the Clock screen) |
| **Long Press ≥ 1 s** | Restart current animation from Frame 0 |

---

## ⚙️ Configuration Reference

Edit [`include/Config.h`](include/Config.h) to customise hardware pins and features:

```cpp
// ── SCREEN SELECTION ──────────────────────────────────────
// #define USE_SSD1306    // Uncomment for 0.96" SSD1306 OLED
#define USE_SH1106        // Default: 1.3" SH1106 OLED

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1   // -1 = share Arduino reset pin

// ── PIN DEFINITIONS ───────────────────────────────────────
#define I2C_SDA_PIN   20   // OLED I2C Data
#define I2C_SCL_PIN   21   // OLED I2C Clock
#define TOUCH_PIN      1   // Capacitive touch input
#define BUZZER_PIN     2   // Buzzer PWM output
#define LED_PIN        6   // NeoPixel data line

// ── LED ───────────────────────────────────────────────────
#define NUM_PIXELS     1   // 1 = single onboard LED; increase for strips
#define LED_BRIGHTNESS 60  // 0 (off) – 255 (full brightness)

// ── TOUCH ─────────────────────────────────────────────────
#define TOUCH_ACTIVE_LEVEL HIGH  // HIGH for most capacitive sensors
#define DEBOUNCE_TIME      50    // ms — ignore bounces shorter than this
#define LONG_PRESS_TIME    1000  // ms — threshold for long-press reset

// ── SOUND ─────────────────────────────────────────────────
#define SOUND_ENABLED  true  // Set false to silence all buzzer output
```

### PlatformIO Build Environments

Defined in [`platformio.ini`](platformio.ini):

| Environment | Board | Use for |
|---|---|---|
| `esp32dev` | ESP32 DevKit V1 | Standard 30/38-pin ESP32 |
| `esp32c3` | DFRobot Beetle ESP32-C3 | ESP32-C3 Supermini / compact boards |
| `pico_w` | Raspberry Pi Pico W | RP2040-based Pico W |

```bash
pio run -e esp32c3              # Build for ESP32-C3
pio run -e esp32dev             # Build for standard ESP32
pio run -e pico_w               # Build for Pico W
pio run -e esp32c3 -t upload    # Build and flash directly
```

---

## 🏗️ Building from Source

```bash
# 1. Install PlatformIO CLI
pip install platformio

# 2. (Optional) Install Pillow for builder GUI icon
pip install pillow

# 3. Clone the repository
git clone https://github.com/YOUR_USERNAME/OLO_Show.git
cd OLO_Show

# 4. Build firmware (choose your target)
pio run -e esp32c3        # ESP32-C3 Supermini
pio run -e esp32dev       # Standard ESP32
pio run -e pico_w         # Pico W

# 5. Build and flash in one command
pio run -e esp32c3 -t upload
```

### Repackage the Builder EXE (Windows)

```bash
# Option A — use the provided batch file
build_exe.bat

# Option B — manually with PyInstaller
pip install pyinstaller
pyinstaller --noconfirm --onefile --windowed --name "OLO_Show_Builder" build_firmware.py
# Output: dist/OLO_Show_Builder.exe
```

---

## 🔧 Troubleshooting

| Problem | Solution |
|---|---|
| **COM port not found in browser** | Use a data USB cable (not charge-only). Install CP210x or CH340 drivers for your board. |
| **WebSerial not available** | Use Chrome or Edge. Open `index.html` via `localhost` or HTTPS — `file://` can block WebSerial. |
| **Flash fails or times out** | Hold BOOT while clicking Connect. Try a shorter cable or lower baud rate. |
| **`firmware_custom.bin` exceeds 3 MB** | Reduce animation slots, lower FPS, or trim video length. |
| **Black screen after flash** | Check wiring. Confirm the correct OLED driver (`SH1106` vs `SSD1306`) in `Config.h`. |
| **No sound from buzzer** | Ensure `SOUND_ENABLED true` and `BUZZER_PIN` matches wiring in `Config.h`. |
| **Touch sensor not responding** | Verify `TOUCH_PIN`, check 3.3 V power to sensor, and confirm `TOUCH_ACTIVE_LEVEL HIGH`. |
| **PlatformIO command not found** | Run `pip install platformio` and ensure `pio` is on the system PATH. |
| **Python script crashes on launch** | Ensure Python 3.8+ is installed. Run `pip install pillow` for icon support. |
| **LED not lighting / wrong colour** | Verify `LED_PIN` and `NUM_PIXELS` in `Config.h`. Check NeoPixel power supply. |
| **GIF not converting properly** | Ensure `omggif.js` is in the **same folder** as `index.html`. |

---

## 📄 License

This project is open source. See [LICENSE](LICENSE) for details.

---

## 🙏 Credits

- **OLO Show** — MicroMaker Labs
- OLED drivers: [Adafruit SSD1306](https://github.com/adafruit/Adafruit_SSD1306) & [Adafruit SH110X](https://github.com/adafruit/Adafruit_SH110X)
- NeoPixel: [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel)
- GIF decoding: [omggif.js](https://github.com/deanm/omggif)
- Built with [PlatformIO](https://platformio.org/) & [Arduino Framework](https://www.arduino.cc/)

