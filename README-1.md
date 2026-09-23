# Smart Automation for Thermal Management System

**SIH Problem Statement:** Modifications to improve the reliability, efficiency, and lifespan of electrical and electronic equipment and systems under subzero temperature and low-pressure conditions in High Altitude Areas (HAA) and Super High Altitude Areas (SHAA) of Ladakh.

## Overview

This prototype is an autonomous thermal management unit that monitors ambient temperature and pressure, automatically heats a protected component when it gets dangerously cold, and logs real experimental data during each cooldown cycle — giving measurable evidence of how the system behaves under simulated high-altitude, subzero conditions.

## Components Required

| Component | Qty | Purpose |
|---|---|---|
| Arduino UNO | 1 | Main controller |
| BMP280 module | 1 | Temperature + pressure sensing (I2C) |
| DS3231 RTC module | 1 | Real-time date/time for logging |
| 16×2 I2C LCD | 1 | Live status display |
| MicroSD card module | 1 | Interface to write CSV log |
| MicroSD card (≤32GB, FAT16/FAT32) | 1 | Storage medium |
| N-channel logic-level MOSFET (e.g., IRLZ44N) | 1 | Switches heat pad on/off |
| DC heating pad | 1 | Heating element |
| Green LED | 1 | "Safe/Normal" indicator |
| Red LED | 1 | "Heating/Affected" indicator |
| 220 Ω resistor | 2 | Current-limiting for LEDs |
| 10 kΩ resistor | 1 | MOSFET gate pulldown |
| Voltage regulator (LM7805 or buck converter) | 1 | Steps down supply to clean 5V |
| External power supply (9V/12V battery pack) | 1 | Powers heat pad + regulator |
| Jumper wires | ~20–30 | All connections |
| Breadboard or perfboard | 1 | Prototyping base |

**Optional:** insulated enclosure, heatsink/thermal paste near MOSFET, multimeter, freezer/dry-ice box for subzero testing.

## Circuit Connections

**Power**
- External supply → Voltage regulator → 5V out → Arduino 5V + all module VCCs
- External supply (raw) → Heat pad → MOSFET Drain
- MOSFET Source → common GND (all grounds — battery, regulator, Arduino — must be tied together)

**MOSFET Gate Control**
- Arduino D9 → 220Ω resistor → MOSFET Gate
- MOSFET Gate → 10kΩ resistor → GND (pulldown)

**I2C Bus** (BMP280 + DS3231 RTC + LCD share these lines)
- SDA → A4, SCL → A5, VCC → 5V, GND → GND
- No address conflicts: BMP280 = 0x76/0x77, RTC = 0x68, LCD = 0x27

**MicroSD Module (SPI)**
- MOSI → D11, MISO → D12, SCK → D13, CS → D4, VCC → 5V, GND → GND

**LEDs**
- D5 → 220Ω → Green LED → GND
- D6 → 220Ω → Red LED → GND

### Pin Summary

| Arduino Pin | Connected To |
|---|---|
| A4 (SDA) | BMP280, RTC, LCD |
| A5 (SCL) | BMP280, RTC, LCD |
| D4 | SD module CS |
| D5 | Green LED |
| D6 | Red LED |
| D9 | MOSFET Gate |
| D11 / D12 / D13 | SD module MOSI / MISO / SCK |
| 5V | Regulator output, all VCCs |
| GND | Common ground |

## How It Works

### 1. Heater control (bang-bang with hysteresis)
- Temp ≤ −10°C → Heater turns **ON** (fixed sustained PWM duty, not full power)
- Heater stays ON until temp reaches **+10°C** → turns **OFF**
- The wide gap between thresholds prevents rapid on/off chatter

### 2. Recording control (event-triggered, separate from heater)
- Once the heater turns OFF (just reached +10°C), the system arms itself
- As temp naturally falls and crosses **0°C**, recording starts: Date, Time, Temp, Pressure logged to SD every **1 second**
- Recording runs as one continuous session through the 0°C to −10°C band
- When temp reaches −10°C again, recording stops, heater turns back ON, and the cycle repeats
- Each session is **appended** to `LOG.CSV` — previous data is never overwritten

### 3. LCD Display
- Line 1: live temperature and pressure
- Line 2: heater state (ON/OFF), recording state (REC), and system status:
  - **SAFE** — stable, no heating needed
  - **AFFECTED** — heater currently active (extreme cold)
  - **FAULT** — sensor not responding

## Data Log Format

`LOG.CSV` on the SD card:

```
Date,Time,Temp_C,Pressure_kPa
23/09/2026,10:15:01,-4.2,64.8
23/09/2026,10:15:02,-4.6,64.7
...
```

## Setup Instructions

1. Install required libraries via Arduino IDE Library Manager:
   - `Adafruit_BME280`
   - `Adafruit_Sensor`
   - `RTClib`
   - `LiquidCrystal_I2C`
   - `SD` (built-in)
2. Wire the circuit as described above — test I2C devices, SD card, and MOSFET+heat pad separately before combining.
3. Upload `thermal_management_system.ino` to the Arduino UNO.
4. Format the microSD card as FAT16/FAT32 before inserting.
5. Power on, confirm LCD shows live readings, and place the sensor/heat pad assembly in a cold environment (freezer or insulated cold box) to test the full cycle.
6. After testing, remove the SD card and open `LOG.CSV` in Excel/Sheets to plot temperature vs. time graphs for your report.

## Tuning Notes

- `HEATER_ON_TEMP`, `HEATER_OFF_TEMP`, `RECORD_START_TEMP`, and `HEAT_DUTY` are all adjustable constants at the top of the sketch — tune based on your actual heat pad's power and insulation.
- BMP280 measures **ambient** conditions where it's placed; it does not create low pressure. To simulate actual low-pressure (high-altitude) conditions, use a sealed vacuum chamber (vacuum desiccator or vacuum-sealer container) with a pump, and let BMP280 confirm the pressure achieved.

## Project Context

This prototype demonstrates a closed-loop, autonomous approach to protecting sensitive electronics from subzero temperatures — directly relevant to Army/DRDO field equipment deployed in Ladakh's HAA/SHAA regions, where manual intervention is often impractical.
