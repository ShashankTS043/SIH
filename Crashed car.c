/**
 * V2V DEMO - APPLICATION 2: CRASHED CAR (Robust Listening Version)
 * ----------------------------------------------------------------
 * Role: This vehicle detects a physical crash and enters a strict
 * broadcast-then-listen cycle to ensure the ambulance ACK is received.
 *
 * HARDWARE (as per your spec):
 * - ESP32
 * - LoRa SX1278
 * - MPU6050
 * - NEO-8M GPS Module  // <-- ADDED
 * - L298N Motor Driver
 * - I2C LCD Display
 * - LED
 */

// LIBRARIES
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <TinyGPS++.h>       // <-- ADDED for GPS parsing
#include <SoftwareSerial.h>  // <-- ADDED for GPS communication

// PIN DEFINITIONS (as per your table)
// LoRa
#define LORA_MOSI 23
#define Lora_MISO 19
#define LORA_SCK  18
#define LORA_NSS  5
#define LORA_RST  14
#define LORA_DIO0 26
// MPU6050 & LCD (I2C)
#define I2C_SDA 21
#define I2C_SCL 22
// GPS (Software Serial)
#define GPS_RX_PIN 16 // ESP32's RX, connect to GPS TX
#define GPS_TX_PIN 17 // ESP32's TX, connect to GPS RX
// Indicators
#define LED_PIN 0

// CONFIGURATION
const char* VEHICLE_ID = "crashed_car_01";
const float CRASH_THRESHOLD_G = 3.0;
const uint32_t GPS_BAUD_RATE = 9600;

// I2C DEVICE ADDRESSES
const int LCD_ADDR = 0x27;
const int MPU_ADDR = 0x68;

// GLOBAL VARIABLES
Adafruit_MPU6050 mpu;
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
TinyGPSPlus gps;                                // <-- ADDED: GPS object
SoftwareSerial ss(GPS_RX_PIN, GPS_TX_PIN);      // <-- ADDED: Software serial for GPS

enum State { NORMAL, CRASH_SEQUENCE_ACTIVE, ACK_RECEIVED };
State currentState = NORMAL;

volatile bool newPacketReceived = false;
char receivedPacketBuffer[256];
const long LISTEN_WINDOW_MS = 2000;

// Store the last known coordinates
float lastKnownLat = 0.0;
float lastKnownLon = 0.0;
bool gpsFixAcquired = false;

// =================================================================
// HELPER FUNCTION PROTOTYPES
// =================================================================
void updateGps();
void onReceive(int packetSize);

// =================================================================
// SETUP
// =================================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  // Initialize Software Serial for GPS
  ss.begin(GPS_BAUD_RATE);

  // Initialize I2C and Devices
  Wire.begin(I2C_SDA, I2C_SCL);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(VEHICLE_ID);
  lcd.setCursor(0, 1);
  lcd.print("GPS: Searching...");

  if (!mpu.begin(MPU_ADDR)) {
    while (1);
  }

  // Initialize LoRa Module
  LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    while (1);
  }
  LoRa.onReceive(onReceive);
  LoRa.receive();

  // Initialize Indicator Pin
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
}

// =================================================================
// MAIN LOOP
// =================================================================
void loop() {
  switch (currentState) {
    case NORMAL:
      updateGps(); // <-- ADDED: Continuously check for new GPS data
      checkForCrash();
      break;

    case CRASH_SEQUENCE_ACTIVE:
      handleCrashSequence();
      break;

    case ACK_RECEIVED:
      // Do nothing. The process is complete.
      break;
  }
}

// =================================================================
// LORA CALLBACK (ISR)
// =================================================================
void onReceive(int packetSize) {
  if (packetSize == 0 || packetSize > sizeof(receivedPacketBuffer) - 1) return;
  int i = 0;
  while (LoRa.available()) {
    receivedPacketBuffer[i] = (char)LoRa.read();
    i++;
  }
  receivedPacketBuffer[i] = '\0';
  newPacketReceived = true;
}

// =================================================================
// HELPER FUNCTIONS
// =================================================================

/**
 * @brief Processes GPS data from the software serial port.
 * This function reads available characters from the GPS module, feeds them to the
 * TinyGPS++ object, and updates global location variables if a valid location is found.
 */
void updateGps() {
  while (ss.available() > 0) {
    if (gps.encode(ss.read())) {
      if (gps.location.isValid()) {
        lastKnownLat = gps.location.lat();
        lastKnownLon = gps.location.lng();
        if (!gpsFixAcquired) {
          gpsFixAcquired = true;
          lcd.setCursor(0, 1);
          lcd.print("Status: NORMAL  "); // Overwrite GPS searching message
        }
      }
    }
  }
}

void handleReceivedPacket() {
  newPacketReceived = false;
  if (currentState != CRASH_SEQUENCE_ACTIVE) return;

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, receivedPacketBuffer);

  if (error) {
    return;
  }

  const char* type = doc["type"];
  if (strcmp(type, "ambulance_ack") == 0) {
      Serial.print("Received Packet: ");
      Serial.println(receivedPacketBuffer);
      Serial.println(">>> Halting Sequence <<<");
      currentState = ACK_RECEIVED;

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("ACK RECEIVED!");
      lcd.setCursor(0, 1);
      lcd.print("Help is on way!");
  }
}

void checkForCrash() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float totalG = sqrt(pow(a.acceleration.x, 2) + pow(a.acceleration.y, 2) + pow(a.acceleration.z, 2)) / 9.81;

  if (totalG > CRASH_THRESHOLD_G) {
    digitalWrite(LED_PIN, HIGH);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("CRASH DETECTED!");
    lcd.setCursor(0, 1);
    lcd.print("Calling for help");

    currentState = CRASH_SEQUENCE_ACTIVE;
  }
}

void handleCrashSequence() {
  while (currentState == CRASH_SEQUENCE_ACTIVE) {
    broadcastCrashAlert();

    unsigned long listenStartTime = millis();

    while (millis() - listenStartTime < LISTEN_WINDOW_MS) {
      if (newPacketReceived) {
        handleReceivedPacket();
        if (currentState == ACK_RECEIVED) {
          return;
        }
      }
    }
  }
}


void broadcastCrashAlert() {
  StaticJsonDocument<200> doc;
  doc["type"] = "crash_alert";
  doc["from"] = VEHICLE_ID;
  doc["lat"] = lastKnownLat; // <-- UPDATED: Use real-time GPS data
  doc["lon"] = lastKnownLon; // <-- UPDATED: Use real-time GPS data

  String jsonString;
  serializeJson(doc, jsonString);

  Serial.print("Broadcasted Packet: ");
  Serial.println(jsonString);

  LoRa.beginPacket();
  LoRa.print(jsonString);
  LoRa.endPacket();

  LoRa.receive();
}
