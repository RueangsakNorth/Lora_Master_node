#define HELTEC_POWER_BUTTON
#include <heltec_unofficial.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define FREQUENCY       433.175
#define TX_POWER_DBM    10
#define TX_INTERVAL_MS  5000

#define ONE_WIRE_PIN 26  // ✅ ใช้ GPIO21 แทน 32
#define NODE_ID "NodeA"

OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);

unsigned long lastTX = 0;

// ===== OLED Power Control =====
void VextON() {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

void displayReset() {
  pinMode(RST_OLED, OUTPUT);
  digitalWrite(RST_OLED, HIGH); delay(1);
  digitalWrite(RST_OLED, LOW);  delay(1);
  digitalWrite(RST_OLED, HIGH); delay(1);
}

void setup() {
  Serial.begin(115200);
  VextON();
  displayReset();
  heltec_setup();

  sensors.begin();

  Serial.println("✅ Heltec TX with DS18B20 Ready");

  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "TX with DS18B20...");
  display.display();

  if (radio.begin() != RADIOLIB_ERR_NONE) {
    display.drawString(0, 16, "❌ LoRa Init Failed");
    display.display();
    while (true);
  }

  radio.setFrequency(FREQUENCY);
  radio.setOutputPower(TX_POWER_DBM);
}

void loop() {
  heltec_loop();

  if (millis() - lastTX >= TX_INTERVAL_MS) {
    sensors.requestTemperatures();
    float temp = sensors.getTempCByIndex(0);

    if (temp == DEVICE_DISCONNECTED_C) {
      Serial.println("❌ DS18B20 not detected");
      display.clear();
      display.drawString(0, 0, "❌ DS18B20 Fail");
      display.display();
      lastTX = millis();
      return;
    }

    String message = "[" NODE_ID "] Temp: " + String(temp, 1) + "°C";

    Serial.println("📡 TX: " + message);

    heltec_led(true);
    int state = radio.transmit(message.c_str());
    heltec_led(false);

    display.clear();
    if (state == RADIOLIB_ERR_NONE) {
      display.drawString(0, 0, "✅ Sent!");
      display.drawString(0, 16, "T: " + String(temp, 1) + " °C");
    } else {
      display.drawString(0, 0, "❌ Send failed");
    }
    display.display();

    lastTX = millis();
  }

  delay(10);
}
