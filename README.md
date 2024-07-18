# Medibox Dashboard

Medibox Dashboard is an ESP32-based smart medicine box system that integrates various sensors and actuators to manage medication schedules effectively.

## Features

- **Real-time Clock:** Synchronized with NTP server for accurate timekeeping.
- **Temperature and Humidity Monitoring:** Uses DHT22 sensor to monitor environmental conditions.
- **Light Intensity Detection:** Utilizes LDR sensors for ambient light intensity measurement.
- **Alarm System:** Configurable alarms to notify medication times.
- **MQTT Integration:** Communicates with MQTT broker for remote monitoring and control.
- **Servo Motor Control:** Controls the angle of a servo motor based on environmental factors.

## Hardware Components

- ESP32 Dev Board
- DHT22 Temperature and Humidity Sensor
- LDR Sensors
- SSD1306 OLED Display
- Servo Motor
- Buzzer and LEDs for alarms

## Software Libraries

- Wire.h (I2C Communication)
- Adafruit_GFX.h and Adafruit_SSD1306.h (OLED Display)
- DHTesp.h (DHT22 Sensor)
- WiFi.h and PubSubClient.h (WiFi and MQTT Communication)
- ESP32Servo.h (Servo Motor Control)
- NTPClient.h and WiFiUdp.h (NTP Client for time synchronization)

## Setup Instructions

1. **Hardware Setup:** Connect sensors (DHT22, LDRs), OLED display, servo motor, LEDs, and buzzer to the ESP32 board as per the circuit diagram.
2. **Software Installation:**
   - Install necessary libraries using Arduino Library Manager.
   - Open the `Medibox_Dashboard.ino` file in Arduino IDE.
   - Modify WiFi credentials (`SSID` and `password`) in the `setup()` function to connect to your local WiFi network.
3. **Compile and Upload:** Compile the sketch and upload it to your ESP32 board.
4. **Usage:** After uploading, the OLED display will show the system status. Use the push buttons (`PB_UP`, `PB_DOWN`, `PB_OK`, `PB_CANCEL`) to navigate through the menu and set alarms as needed.

## MQTT Topics

- **Medibox-Minimum-Angle:** Sets the minimum angle for servo motor control.
- **Medibox-Controlling-Factor:** Adjusts the controlling factor based on MQTT input.
- **Medibox-Select-Medicine:** Selects a specific medicine based on MQTT message.
- **Medibox-SCH-ON:** Enables or disables scheduled medication times.


## Acknowledgments

- **Adafruit Industries:** Libraries for OLED display and graphics.
- **Open Source Community:** Contributions to libraries and forums that helped in the development.
