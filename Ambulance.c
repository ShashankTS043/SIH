/**
 * V2V DEMO - APPLICATION 2: AMBULANCE (Updated Logic)
 * -------------------------------------------
 * Role: This vehicle listens for "crash_alert" messages via LoRa.
 * When an alert is received, it broadcasts an "ambulance_ack"
 * message repeatedly for 15 seconds and then stops.
 *
 * HARDWARE (as per your spec):
 * - ESP32
 * - LoRa SX1278
 * - I2C LCD Display
 * - Buzzer
 */

// LIBRARIES
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>

// PIN DEFINITIONS (as per your table)
// LoRa
#define LORA_MOSI 23
#define LORA_MISO 19
#define LORA_SCK  18
#define LORA_NSS  5
#define LORA_RST  14
#define LORA_DIO0 26
// LCD (I2C)
#define I2C_SDA 21
#define I2C_SCL 22
// Indicators
#define BUZZER_PIN 2

// CONFIGURATION
const char* VEHICLE_ID = "ambulance_01";
const long ACK_BROADCAST_DURATION_MS = 15000; // Broadcast for 15 seconds
const long ACK_BROADCAST_INTERVAL_MS = 500;   // Send an ACK every 500ms

// I2C DEVICE ADDRESSES
const int LCD_ADDR = 0x27;

// GLOBAL VARIABLES
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);

enum State { PATROLLING, RESPONDING };
State currentState = PATROLLING;

volatile bool newPacketReceived = false;
char receivedPacketBuffer[256];
unsigned long respondingStartTime = 0;
unsigned long lastAckSendTime = 0;

// =================================================================
// SETUP FUNCTION
// =================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("Ambulance Vehicle Initializing...");

  // --- Initialize I2C and LCD
  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(VEHICLE_ID);
  lcd.setCursor(0, 1);
  lcd.print("Status: PATROL");

  // --- Initialize LoRa Module
  LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) { // Must be same frequency as crashed car
    Serial.println("Starting LoRa failed!");
    lcd.setCursor(0, 1);
    lcd.print("LoRa FAILED!");
    while (1);
  }
  LoRa.onReceive(onReceive);
  LoRa.receive();
  Serial.println("LoRa Initialized and in receive mode.");

  // --- Initialize Buzzer Pin
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  Serial.println("Initialization Complete. Listening for alerts...");
}

// =================================================================
// MAIN LOOP
// =================================================================
void loop() {
  // Check if the ISR has set the flag for a new packet
  if (newPacketReceived) {
    handleReceivedPacket();
  }

  // If we are in the RESPONDING state, handle broadcasting and buzzer
  if (currentState == RESPONDING) {
    // Check if we are still within the 15-second broadcast window
    if (millis() - respondingStartTime < ACK_BROADCAST_DURATION_MS) {
      
      // Beep the buzzer intermittently
      digitalWrite(BUZZER_PIN, HIGH);
      delay(100);
      digitalWrite(BUZZER_PIN, LOW);
      
      // Check if it's time to send the next ACK broadcast
      if (millis() - lastAckSendTime > ACK_BROADCAST_INTERVAL_MS) {
        sendAmbulanceAck();
        lastAckSendTime = millis();
      }
    } else {
      // After 15 seconds, stop the buzzer and do nothing further.
      digitalWrite(BUZZER_PIN, LOW);
    }
  }
}

// =================================================================
// LORA CALLBACK FUNCTION (ISR CONTEXT)
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
// PACKET PROCESSING FUNCTION (MAIN LOOP CONTEXT)
// =================================================================
void handleReceivedPacket() {
  newPacketReceived = false;

  Serial.print("Received LoRa Packet: ");
  Serial.println(receivedPacketBuffer);

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, receivedPacketBuffer);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }
  
  const char* type = doc["type"];
  if (strcmp(type, "crash_alert") == 0) {
    // Only respond if we are not already responding
    if (currentState == PATROLLING) {
      Serial.println("Crash Alert Received! Starting 15-second response broadcast.");
      currentState = RESPONDING;
      respondingStartTime = millis(); // Start the 15-second timer

      // Update display
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("CRASH ALERT RX'd!");
      lcd.setCursor(0, 1);
      lcd.print("Status: RESPOND");
    }
  }
}

// =================================================================
// HELPER FUNCTIONS
// =================================================================
void sendAmbulanceAck() {
  StaticJsonDocument<200> doc;
  doc["type"] = "ambulance_ack";
  doc["from"] = VEHICLE_ID;
  doc["status"] = "on_the_way";
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  // Send the ACK packet
  LoRa.beginPacket();
  LoRa.print(jsonString);
  LoRa.endPacket();
  
  // Go back to listening mode
  LoRa.receive();  
  
  Serial.print("Sent Ambulance ACK: ");
  Serial.println(jsonString);
}
