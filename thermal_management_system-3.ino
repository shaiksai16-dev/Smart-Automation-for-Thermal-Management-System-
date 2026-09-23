/*
  SMART AUTOMATION FOR THERMAL MANAGEMENT SYSTEM
  SIH Problem Statement: Reliability of electronics under subzero temp
  and low-pressure conditions in HAA/SHAA (Ladakh)

  CONTROL LOGIC:
  1) Heater (bang-bang with wide hysteresis):
     - Temp <= -10C  -> Heater ON
     - Heater stays ON until Temp >= +10C -> Heater OFF
     (wide gap prevents rapid on/off chatter)

  2) Recording (event-triggered, separate from heater):
     - After heater turns OFF (temp just reached +10C), system "arms"
     - As temp naturally falls and crosses <= 0C, recording STARTS:
         logs Date, Time, Temp, Pressure to SD every 1 second
     - Recording continues as ONE continuous session through the
       0 to -10C band (not re-triggered by minor fluctuations)
     - When temp reaches -10C again, heater turns back ON and that
       recording session ends. Data is APPENDED to the same CSV file,
       previous sessions are never overwritten.

  Components:
  - Arduino UNO
  - BMP280 (temp + pressure, I2C)
  - DS3231 RTC (I2C) - date/time for logging
  - 16x2 I2C LCD - live status
  - MicroSD module (SPI) - CSV logging
  - N-channel logic-level MOSFET - heat pad switch (gate D9)
  - 2 LEDs: Green (D5, normal/safe) / Red (D6, heating/affected)
*/

#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h> 
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>

// ---------- Pins ----------
#define MOSFET_PIN     9
#define LED_GREEN      5
#define LED_RED        6
#define SD_CS_PIN      4
Adafruit_BME280 bmp;
RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2); 
const float HEATER_ON_TEMP    = -10.0; 
const float HEATER_OFF_TEMP   =  10.0; 
const float RECORD_START_TEMP =   0.0; 
const int   HEAT_DUTY         = 200;   
const unsigned long RECORD_INTERVAL = 1000; 

bool bmpOK = false;
bool rtcOK = false;
bool sdOK  = false;

bool heaterOn        = false;
bool recordingArmed  = false; // true once heater has switched OFF, waiting to reach 0C
bool recording       = false; // true while actively logging in the 0 to -10C band

unsigned long lastRecordTime = 0;

void setup() {
  Serial.begin(9600);

  pinMode(MOSFET_PIN, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  analogWrite(MOSFET_PIN, 0);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Thermal Mgmt Sys");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");

  bmpOK = bmp.begin(0x76) || bmp.begin(0x77);
  if (!bmpOK) Serial.println("BMP280 not found!");

  rtcOK = rtc.begin();
  if (!rtcOK) {
    Serial.println("RTC not found!");
  } else if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  sdOK = SD.begin(SD_CS_PIN);
  if (!sdOK) {
    Serial.println("SD card init failed!");
  } else if (!SD.exists("LOG.CSV")) {
    File f = SD.open("LOG.CSV", FILE_WRITE);
    if (f) {
      f.println("Date,Time,Temp_C,Pressure_kPa");
      f.close();
    }
  }

  delay(1500);
  lcd.clear();
}

void loop() {
  float temp = bmpOK ? bmp.readTemperature() : NAN;
  float pressure = bmpOK ? (bmp.readPressure() / 1000.0) : NAN; // Pa -> kPa

  bool sensorFault = !bmpOK || isnan(temp);

  // ---------- Heater state machine (bang-bang, hysteresis) ----------
  if (!sensorFault) {
    if (temp <= HEATER_ON_TEMP) {
      heaterOn = true;
      // entering deep cold: end any active recording session, and
      // require a full new cycle (heater off + reach +10) before rearming
      recording = false;
      recordingArmed = false;
    } else if (heaterOn && temp >= HEATER_OFF_TEMP) {
      heaterOn = false;
      recordingArmed = true; // ready to start recording once it cools to 0C
    }
  }

  analogWrite(MOSFET_PIN, heaterOn ? HEAT_DUTY : 0);

  // ---------- Recording state machine ----------
  if (!sensorFault && recordingArmed && !recording &&
      temp <= RECORD_START_TEMP && temp > HEATER_ON_TEMP) {
    recording = true;
    recordingArmed = false; // consumed; won't re-trigger until next full cycle
  }

  // ---------- LEDs & system status ----------
  bool affected = heaterOn || sensorFault; // extreme cold or sensor issue
  digitalWrite(LED_RED, affected ? HIGH : LOW);
  digitalWrite(LED_GREEN, affected ? LOW : HIGH);
  String sysStatus = sensorFault ? "FAULT" : (affected ? "AFFECTED" : "SAFE");

  // ---------- LCD ----------
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temp, 1);
  lcd.print("C P:");
  lcd.print(pressure, 0);
  lcd.print("   ");

  lcd.setCursor(0, 1);
  lcd.print(heaterOn ? "H:ON " : "H:OFF ");
  lcd.print(recording ? "REC " : "");
  lcd.print(sysStatus);
  lcd.print("        "); // pad to clear leftover characters

  // ---------- Serial debug ----------
  Serial.print("Temp: "); Serial.print(temp);
  Serial.print(" Pressure(kPa): "); Serial.print(pressure);
  Serial.print(" Heater: "); Serial.print(heaterOn ? "ON" : "OFF");
  Serial.print(" Recording: "); Serial.print(recording ? "YES" : "NO");
  Serial.print(" Status: "); Serial.println(sysStatus);

  // ---------- SD Logging (only while `recording` is true, every 1s) ----------
  if (recording && sdOK && rtcOK && (millis() - lastRecordTime >= RECORD_INTERVAL)) {
    lastRecordTime = millis();
    DateTime now = rtc.now();

    File f = SD.open("LOG.CSV", FILE_WRITE); // FILE_WRITE appends, never overwrites
    if (f) {
      char dateStr[12];
      char timeStr[12];
      sprintf(dateStr, "%02d/%02d/%04d", now.day(), now.month(), now.year());
      sprintf(timeStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());

      f.print(dateStr); f.print(",");
      f.print(timeStr); f.print(",");
      f.print(temp); f.print(",");
      f.println(pressure);

      f.close();
    } else {
      Serial.println("Error writing to SD card");
    }
  }

  delay(200); // fast enough loop; actual recording still gated to exactly 1s by RECORD_INTERVAL
}