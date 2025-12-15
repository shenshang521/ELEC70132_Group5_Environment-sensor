Environmental Sensor Board Files
Imperial College London, Dep. of Electrical and Electronic Engineering,
ELEC70132 - Laboratory in Sensor Systems, Group 5
------------------------------------------------------------------------
# ESP32 Environmental Sensor Hub & IoT Data System

## Project Overview
This repository contains the source code and documentation for an ESP32-based Environmental Sensor Hub. This project is submitted as part of the MSc coursework at **Imperial College London**.

The system is a complete **IoT solution** that integrates:
1.  **Edge Device (ESP32):** Monitors environmental conditions (BME680), air quality, distance (ToF), and geolocation (GPS).
2.  **Web Interface:** Hosted directly on the ESP32 for real-time visualization.
3.  **PC Integration (Python):** A desktop application to wirelessly collect sensor data into a database and export it for analysis (Excel/CSV).

## Key Features
*   **Multi-Sensor Fusion:** Reads Temperature, Humidity, Pressure, Gas Resistance (IAQ), Distance, and GPS location simultaneously.
*   **Real-Time Web Dashboard:** Uses **Server-Sent Events (SSE)** to push data to browsers without page refreshes.
*   **Wireless Data Logging (Python):** Python scripts allow for long-term data collection over WiFi into an SQLite database.
*   **Data Analysis Ready:** Includes a tool to export database records to `.csv` format for Excel/MATLAB analysis.
*   **Local Backup:** Automated CSV logging to an onboard SD card for redundancy.
*   **Visual Feedback:** OLED display showing status, IP address, and sensor readings.

## Hardware Requirements
*   **Microcontroller:** ESP32 Development Board
*   **Sensors:** Adafruit BME680, VL53L4CX (ToF), GPS Module (I2C)
*   **Display:** SSD1306 128x64 OLED (I2C)
*   **Storage:** MicroSD Card Module
*   **Connectivity:** WiFi (2.4GHz)

## Dependencies (Arduino IDE)
Please install the following libraries via the Arduino Library Manager:
*   ESPAsyncWebServer
*   AsyncTCP
*   Adafruit SSD1306
*   Adafruit GFX
*   Adafruit BME680 Library
*   Adafruit GPS Library
*   VL53L4CX

## Pin Configuration
| Component | ESP32 Pin | Protocol | Notes |
|-----------|-----------|----------|-------|
| **OLED Display** | SDA / SCL | I2C | Address: 0x3D |
| **BME680** | SDA / SCL | I2C | Default Address |
| **VL53L4CX** | SDA / SCL | I2C | Default Address |
| **GPS Module** | SDA / SCL | I2C | Address: 0x10 |
| **SD Card CS** | GPIO 5 | SPI | Chip Select |
| **SD Card SCK** | GPIO 18 | SPI | VSPI |
| **SD Card MISO** | GPIO 19 | SPI | VSPI |
| **SD Card MOSI** | GPIO 23 | SPI | VSPI |

## Setup Instructions

### ESP32 Configuration
1. Open `src/main/secrets.h`.
2. Replace `YOUR_WIFI_SSID` and `YOUR_WIFI_PASSWORD` with your actual WiFi credentials before flashing the code.
3. Upload the code to your ESP32.
4. Note the IP Address displayed on the OLED screen after startup.

### Python Configuration
1. Open `python_scripts/receive.py`.
2. Find the `ESP32_IP` variable (or URL setting).
3. Update it to match the IP address shown on your ESP32's OLED screen.
4. Run the script to start logging data.

## Project Structure
```text
ESP32_Sensor_Project/
├── src/
│   └── main/                  # Arduino Sketch Folder
│       ├── main.ino           # Main Firmware
│       ├── secrets.h          # WiFi Credentials (Template)
│       └── index_html.h       # Web Frontend (HTML/JS)
├── python_scripts/
│   ├── receive.py             # Rx Data from ESP32 -> SQLite DB
│   └── export_data.py         # Convert DB -> CSV/Excel
├── docs/
│   └── Environmental Sensor Report - Final.pdf             # Coursework Report
├── hardware/
│   └── Environmental_PCB_Group5.epro  # PCB Design 
└── README.md                  # Documentation
├── previous code/             # Development History & Iterations
│   ├── Cali_friday            # Initial sensor calibration tests
│   ├── Calibriation_Environment_sensor # Detailed calibration logic
│   ├── Environment_WIFI_SD_HTML # Early integration (Web+SD)
│   ├── environment            # Basic environmental sensor drivers
│   ├── python_scripts         # Legacy/Old Python scripts
│   └── sd_tof                 # Distance sensor & SD 
├── 3D Enclosure/              # Mechanical Design Files (.stl)
│   ├── PCB_Enclosure_Body.stl # Main housing for electronics
│   ├── PCB_Cover.stl          # Lid for the sensor unit
│   ├── Power_bank_body.stl    # Holder for external battery
│   └── Power_bank_cover.stl   # Lid for the battery

The original intention for this project was was to develop a single-PCB
sensor system capable of wired and wireless communication (via a Wi-Fi
connection). All the relevant files for this project can be found in
this public GitHub repository, including the final report.

For more details, feel free to contact: tr525@ic.ac.uk
