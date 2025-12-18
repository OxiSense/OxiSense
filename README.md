# OxiSense

OxiSense is a microcontroller-based wearable system that measures **heart rate (BPM)** and **oxygen saturation (SpO₂)** in real time. The project was developed as part of the master-level course **Microcontroller (CM2036)**, with a focus on **embedded C**, **hardware design**, and **real-time signal processing**.

The goal of OxiSense was to create a simple, portable, and reliable solution for monitoring vital signs during everyday use and physical activity. The system is complemented by a custom **iOS application** (Swift/Xcode) that connects over **Bluetooth**, retrieves logged measurements, and visualizes BPM/SpO₂ trends for long-term analysis.

---

## Key Features

- Real-time **heart rate (BPM)** measurement  
- Real-time **oxygen saturation (SpO₂)** estimation  
- On-device data collection and signal processing  
- **SD-card logging** for long-term measurement storage  
- **Bluetooth** communication with iOS app

---

## Technology Stack

**Embedded**
- Microcontroller: **RISC-V**
- Sensor: **MAX30102** (PPG for pulse + SpO₂)
- Language: **Embedded C**
- Storage: **SD card**
- Connectivity: **Bluetooth**

**Mobile**
- iOS app: **Swift (Xcode)**
- Functionality: data retrieval + visualization

---

## System Overview

1. The MAX30102 sensor captures PPG signals.
2. The RISC-V microcontroller performs on-device processing to estimate BPM and SpO₂.
3. Live values are shown on the LED display.
4. Measurements are logged to an SD card for later analysis.
5. The iOS app connects via Bluetooth to fetch logged data and visualize trends.

---

## Project Status

✅ **Completed student project** — final prototype supports real-time BPM/SpO₂ monitoring with on-device processing, display output, and SD-card logging.  
The repository contains the firmware, hardware documentation, and initial groundwork for future extensions such as improved wireless communication and further mobile-app features.

---

## Demo / Photos

![OxiSense Prototype](https://github.com/user-attachments/assets/835b55a0-af5c-4ba5-8d01-fd2cde003d40)
![OxiSense Prototype](https://github.com/user-attachments/assets/94a89627-dbc9-4ce2-9e7b-0d5143693980)

---

## Repository Structure

> TODO: Update these paths to match your repo

- `firmware/` — embedded C code and drivers  
- `hardware/` — schematics/PCB/mechanical notes  
- `ios-app/` — iOS app (Swift/Xcode)  
- `docs/` — additional documentation, diagrams, and reports  

---

## Getting Started

> TODO: Adjust this section to your build setup

### Firmware
1. Clone the repository
2. Open the firmware project in your toolchain/IDE
3. Build and flash to the target board
4. Connect the MAX30102 sensor and verify live readings on the LED display

### iOS App
1. Open the iOS project in Xcode
2. Select a device (Bluetooth required)
3. Build and run
4. Connect to OxiSense and download logged data

---

## Future Improvements

- Improve filtering and robustness under motion
- Stream live measurements over Bluetooth (in addition to SD logging)
- Expand the iOS app with dashboards and export features
- Power optimization and improved enclosure/wearability

---

## Team

- Patrik Kassir  
- Kristian Yousef  
- Dennis Chawshin  
- Tobias Brasiliero Reyes  
- Romeo Haddad  
- Simon Mahari  
