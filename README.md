# Pocket — ESP32-C3 Low-Power Emoji Pendant

<div align="center">

**A tiny desk companion with animated expressions, accelerometer wake-up, and USB-C charging**

[![Arduino](https://img.shields.io/badge/Arduino-ESP32C3-blue.svg)](https://www.arduino.cc/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

[**中文文档**](README_CN.md)

</div>

<img src="images/jlc-home.png" width="800"/>

## Features

- **Animated Expressions** — 6 emotion GIFs (anger, disdain, horror, excited, sad, idle) played randomly on a 1.47" IPS screen
- **Shake to Wake** — LSM6DS3TRC accelerometer detects motion; auto-dims after 10s of inactivity
- **Charging Animation** — Displays a battery animation when USB-C is plugged in
- **Ultra Low Power** — Screen off during idle, wakes instantly on shake
- **3D-Printed Enclosure** — Cat-ear design with Rhino source files and print-ready STLs

## Gallery

<table>
<tr>
<td><img src="images/image1.png" width="280"/></td>
<td><img src="images/image2.png" width="280"/></td>
<td><img src="images/image3.png" width="280"/></td>
</tr>
<tr>
<td><img src="images/image4.png" width="280"/></td>
<td><img src="images/image5.png" width="280"/></td>
<td><img src="images/image6.png" width="280"/></td>
</tr>
</table>

## Replication Example

<img src="images/example.png" width="800"/>

## Hardware

| Component | Spec |
|-----------|------|
| MCU | ESP32-C3FN4 |
| Display | 1.47" IPS ST7789 (172×320) |
| IMU | LSM6DS3TRC (6-axis) |
| Charging | TP4057, USB-C |
| Battery | 3.7V Li-Po |
| PCB Thickness | 0.8mm |

## Circuit Design

**Schematic**

<img src="images/Schematic.png" width="800"/>

**PCB Layout**

<img src="images/pcb.png" width="800"/>

## Project Structure

```
pocket/
├── Pocket/
│   ├── Arduino/
│   │   ├── Pocket.ino          # Arduino firmware
│   │   └── partitions.csv      # ESP32 partition table
│   ├── filesystem/
│   │   ├── flash.bat           # SPIFFS flash script
│   │   └── FS/
│   │       ├── spiffs.py       # SPIFFS image generator
│   │       ├── UpLoad.py       # Serial upload tool
│   │       └── SPIFFS/         # GIF assets for firmware
│   └── Emoji/                  # After Effects animation sources
├── hardware/
│   ├── model.3dm               # Rhino 3D model
│   ├── shell.stl               # Enclosure (print-ready)
│   └── back-cover.stl          # Back cover (print-ready)
├── libs/                       # Arduino library dependencies
├── images/                     # Photos & screenshots
└── README.md
```

## Quick Start

### 1. Install Arduino Libraries

Extract the archives in `libs/` to your Arduino libraries folder, or install via Arduino Library Manager:
- **Arduino_GFX_Library** — Display driver
- **Adafruit_LSM6DS** — IMU driver
- **AnimatedGIF** — GIF decoder (install via Library Manager)

### 2. Flash Firmware

1. Open `Pocket/Arduino/Pocket.ino` in Arduino IDE
2. Select board: **ESP32C3 Dev Module**
3. Set partition scheme to use the custom `partitions.csv`
4. Upload

### 3. Flash SPIFFS (GIF Assets)

Requires Python. Install the dependency first:

```bash
pip install esptool
```

Then run the flash script:

```bash
cd Pocket/filesystem
flash.bat
```

This generates a SPIFFS image from the GIF files and uploads it to the ESP32-C3.

## Expressions

| Emotion | GIF File | Trigger |
|---------|----------|---------|
| Anger | `anger.gif` | Random on wake |
| Disdain | `disdain.gif` | Random on wake |
| Excited | `excited.gif` | Random on wake |
| Sad | `once.gif` | Random on wake |
| Idle | `twece.gif` | Random on wake |
| Charging | `charge.gif` | USB-C plugged in |

## License

MIT License
