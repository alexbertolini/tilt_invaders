# 👾 Tilt Invaders

**Tilt Invaders** is a 3D Space Invaders arcade game built for the **Waveshare ESP32-S3-LCD-1.28** development board. 

The 1.28-inch round display acts as a window onto an alien desert planet. Using the onboard 6-axis IMU, the camera angle responds to physical board tilt, allowing you to scan the horizon and track incoming waves of invaders in full pseudo-3D.

---

## 🎮 Gameplay & Mechanics

* **Look Around (Tilt Controls):** 
  * **Pitch:** Holding the board flat looks straight down at the ground. Raising the top edge lifts your field of view up to $60^\circ$ above the horizon.
  * **Yaw:** Tilting left or right rotates your view up to $60^\circ$ in either direction.
* **Fire / Start:** Press the onboard **BOOT** button to fire your laser straight at your central crosshair. 
* **Invaders:**
  * Invaders move in pattern formation (shift left, shift right, then drop lower). If an invader lands on the surface, it's **Game Over**.
  * Each invader takes **2 laser hits** to destroy.
  * Level 1 starts with 1 invader, Level 2 with 2, and so on infinitely.
* **Incoming Enemy Bolts:**
  * Invaders fire back directly at your location. A energy bolt expands where the invader was standing and damages you if your crosshair is pointing at that spot when it arrives.
  * You start with 3 shields/bonuses. Taking a hit consumes one bonus; clearing a level restores one bonus (max 10).
* **Recalibration:** Hold the **BOOT** button for **~1.5 seconds** at any time to recalibrate the "level" resting attitude.

---

## 🛠️ Hardware Requirements

* **Board:** [Waveshare ESP32-S3-LCD-1.28](https://www.waveshare.com/esp32-s3-lcd-1.28.htm) (non-touch version)
* **Display Driver:** GC9A01 (240x240 Round IPS LCD)
* **IMU:** QMI8658 (6-axis Inertial Measurement Unit)
* **Control Input:** Onboard `BOOT` button (GPIO 0)

---

## 💻 Software Dependencies & Setup

### 1. Arduino IDE Setup
* Install the **ESP32** board package by Espressif (`v2.0.x` or `v3.0.x - v3.3.x`).
* Select **ESP32S3 Dev Module** under `Tools > Board > esp32`.
* Recommended IDE Settings:
  * **USB CDC On Boot:** Disabled
  * **Flash Size:** 16MB (32Mb)
  * **PSRAM:** OPI PSRAM (Optional; internal SRAM is prioritized)

### 2. Required Libraries
* **[GFX Library for Arduino](https://github.com/moononournation/Arduino_GFX)** by *Moon On Our Nation* (Install via Arduino Library Manager).

> **Note on ESP32 Board Core Compatibility:** 
> If using **ESP32 Arduino Core v3.0 – v3.3**, pair it with `GFX_Library_for_Arduino` **v1.4.x - v1.6.9**. If using **ESP32 Core v3.4+**, ensure your GFX library is updated to the latest available release.

---

## 📁 Project Structure

```text
tilt_invaders/
├── tilt_invaders.ino   # Main loop, hardware init (GC9A01 + QMI8658), and tilt input math
├── inv_game.h          # Game logic, state management, enemy AI, and projectile mechanics
├── inv_render.h        # 3D math and software frame buffer rendering engine
└── inv_sprites.h       # 16-bit pixel art sprite definitions & color data
```

---

## ⚙️ Configuration & Sensitivity Tuning

You can adjust view responsiveness and tilt inversion directly in `tilt_invaders.ino`:

```cpp
// Steering sensitivity multipliers
const float PITCH_GAIN  = 1.5f;   // Lowering/raising top-edge gain
const float YAW_GAIN    = 4.0f;   // Left/right tilt gain

// Invert controls (0 = Normal, 1 = Inverted)
#define INVERT_PITCH 0
#define INVERT_YAW   1

// View smoothing factor (in seconds)
const float VIEW_TAU    = 0.5f;
```

---

## 📜 License

This project is open-source. Feel free to modify, optimize, and share!