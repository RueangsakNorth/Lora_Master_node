#define HELTEC_POWER_BUTTON
#include <heltec_unofficial.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ============ CONFIG =============
#define I2C_SDA         26
#define I2C_SCL         48
#define FREQUENCY       433.175
#define TX_POWER_DBM    10
#define TX_INTERVAL_MS  5000
#define NODE_ID         "NodeA"

// MAX30102
TwoWire customWire = TwoWire(0);
MAX30105 maxSensor;
uint32_t irBuffer[100];
uint32_t redBuffer[100];
int32_t bufferLength;
int32_t spo2;
int8_t validSPO2;
int32_t heartRate;
int8_t validHeartRate;
int BPM = 0;
int SpO2 = 0;

// DS18B20
#define ONE_WIRE_PIN 33
OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature ds18b20(&oneWire);
float bodyTemp = 0.0;

unsigned long lastTX = 0;

// ===== Smooth HR Filter =====
int smoothBPM(int newValue) {
  static int lastBPM = 0;
  if (newValue < 30 || newValue > 180) return lastBPM;
  lastBPM = (0.7 * lastBPM) + (0.3 * newValue);
  return lastBPM;
}

void setup() {
  Serial.begin(115200);

  // I2C สำหรับ MAX30102
  customWire.begin(I2C_SDA, I2C_SCL, 400000);

  // เริ่ม MAX30102
  if (!maxSensor.begin(customWire)) {
    Serial.println("❌ MAX30102 ไม่พบ");
    while (true);
  }
  maxSensor.setup(60, 4, 2, 100, 411, 4096);

  // เริ่ม DS18B20
  ds18b20.begin();

  // เริ่ม LoRa
  heltec_setup();
  if (radio.begin() != RADIOLIB_ERR_NONE) {
    Serial.println("❌ LoRa Init Failed");
    while (true);
  }

  radio.setFrequency(FREQUENCY);
  radio.setOutputPower(TX_POWER_DBM);

  Serial.println("✅ System Ready: MAX30102 + DS18B20 → LoRa");
}

void loop() {
  heltec_loop();

  if (millis() - lastTX >= TX_INTERVAL_MS) {
    lastTX = millis();

    readHeartSensor();
    readTemperature();
    sendData();
  }

  delay(10);
}

void readHeartSensor() {
  unsigned long t = millis();
  while (millis() - t < 500) {
    while (!maxSensor.available()) maxSensor.check();
    maxSensor.nextSample();
  }

  bufferLength = 50;
  for (int i = 0; i < bufferLength; i++) {
    while (!maxSensor.available()) maxSensor.check();
    redBuffer[i] = maxSensor.getRed();
    irBuffer[i] = maxSensor.getIR();
    maxSensor.nextSample();
  }

  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer,
    &spo2, &validSPO2, &heartRate, &validHeartRate);

  BPM = (validHeartRate && heartRate > 30 && heartRate < 180) ? smoothBPM(heartRate) : smoothBPM(0);
  SpO2 = (validSPO2 && spo2 >= 80 && spo2 <= 100) ? spo2 : 0;
}

void readTemperature() {
  ds18b20.requestTemperatures();
  float temp = ds18b20.getTempCByIndex(0);
  if (temp != DEVICE_DISCONNECTED_C) {
    bodyTemp = temp;
  } else {
    Serial.println("❌ DS18B20 ไม่พบ");
    bodyTemp = 0.0;
  }
}

void sendData() {
  String message = "[" NODE_ID "] Temp: " + String(bodyTemp, 1) + "°C, BPM: " + String(BPM) + ", SpO2: " + String(SpO2) + "%";
  Serial.println("📡 TX: " + message);

  heltec_led(true);
  int state = radio.transmit(message.c_str());
  heltec_led(false);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("✅ Sent!");
  } else {
    Serial.println("❌ Send failed");
  }
}
