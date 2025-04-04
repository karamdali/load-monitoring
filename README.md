# Load Monitoring System - an ESP32 web-app over WebSocket server

### Overview

This project is an ESP32-based load monitoring system prototype that measures and records voltage, current, and temperature data suitable for batteries. The system provides real-time monitoring through a web interface with interactive charts and stores historical data in an SQLite database on an SD card.

#### Features 
- Real-time sensor monitoring:
- Voltage measurement (using voltage divider circuit)
- Current measurement (using ACS712 sensor)
- Temperature measurement (using TMP102 sensor)

#### Data visualization:
- Web-based interface with dynamic charts
- Real-time updates via WebSocket

#### Data storage:
- SQLite database for historical data
- SD card storage
#### Network connectivity:
- WiFi connection
- NTP time synchronization
#### User interface:
- 16x2 LCD display for local status
- Web interface accessible from any device

## Hardware Requirements
- ESP32 microcontroller (with 4MB flash `sqlite3` needs 2MB)
- TMP102 temperature sensor
- ACS712 current sensor
- Voltage divider circuit (R1=30Ω, R2=7.5Ω)
- 16x2 I2C LCD display
- SD card + SD card module
- WiFi connectivity

## Pin Configuration


| Component | ESP32 | Pin |
| --------- | -----| ---- |
| I2C       | SDA   | GPIO21 |
|I2C| SCL | GPIO22 |
| SD Card | MISO |	GPIO19 |
| SD Card | MOSI | GPIO23 |
| SD Card | SCK | GPIO18 |
|SD Card | CS |GPIO5 |
|Voltage | Analog input | GPIO35
|Current | Analog input | GPIO39

## Software Requirements
- VS code
- PlatformIO

### Main libraries:
`SparkFunTMP102`
`LiquidCrystal_I2C`
`ESPAsyncWebServer`
`WebSocketsServer`
`ArduinoJson`
`sqlite3`
`SPIFFS`
`SD`

## Installation
- Clone this repository
- Open project in VSCode with PlatformIO extension
- Create `/data` directory and add web interface files `index.html` `sql.html`
- Upload SPIFFS filesystem. Change ESP32 partition scheme using `huge_app.csv` 
- Copy `power.db` to the SD card
- Build & Upload Firmware

## Configuration

Before uploading:
- Set your WiFi credentials in main.cpp:
    ```cpp
    const char* ssid = "YOUR_SSID";
    const char* password = "YOUR_PASSWORD";
    ```
- Adjust NTP settings if not in UTC+3 timezone:
    ```cpp
    const long  gmtOffset_sec = 10800;  //For UTC +3 (Syria) : 3 * 60 * 60 = 10800 
    ```
## Usage

- Power on the ESP32
- Connect to the access point specified in the code
- Open a web browser and navigate to the ESP32's IP address shown on the LCD
#### The web interface will show:

- Real-time charts of voltage, current, and temperature
- Historical data query interface

## Database Structure

The SQLite database (power.db) contains a table with the following structure:
```sql
CREATE TABLE SensorData (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    date TEXT,
    time TEXT,
    voltage REAL,
    current REAL,
    temperature REAL
);
```

## Debugging

The code includes extensive debug output when DEBUGE is defined. To enable debugging:
```cpp
#define DEBUGE 1
```

## Limitations
This is a prototype and not intended for production use

The ESP32's standard partition scheme need adjustment to be able to upload the program to the flash.

The LCD message display function has limited scrolling capabilities

### License

This code is provided AS IS without warranty. Feel free to use and modify it for any purpose.


### Author

Karam Dali - Damascus (02/03/2024)

## Acknowledgments
Thanks to all the open-source library maintainers whose work made this project possible

Special thanks to `MoThunderz` for his awesome YouTube tutorials.

#### For any questions or feedback, please open an issue on this repository.
