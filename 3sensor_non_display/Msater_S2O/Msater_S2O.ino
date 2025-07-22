#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <LoRa.h>
#include <LiquidCrystal_I2C.h>

//--- LCD Setup ---
LiquidCrystal_I2C lcd(0x27, 16, 2); // อาจเป็น 0x3F แล้วแต่โมดูล

//--- LoRa Pins ---
#define LORA_SCK 5
#define LORA_MISO 19
#define LORA_MOSI 27
#define LORA_SS 18
#define LORA_RST 14
#define LORA_DI0 26

//--- WiFi ---
const char* ssid = "@Signal-Free-WIFI";
const char* password = "";

//--- GAS Script ID ---
String GAS_ID = "AKfycby0kRQSuMC7opzQ8_jmGM5P0OI265szdfLFYwgzOyw";

//--- Setup ---
void setup() {
  Serial.begin(115200);

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("LoRa Receiver...");
  delay(1000);
  lcd.clear();

  // LoRa Init
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DI0);

  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed");
    lcd.setCursor(0, 0);
    lcd.print("LoRa failed");
    while (true);
  }

  Serial.println("LoRa Ready");
  lcd.setCursor(0, 0);
  lcd.print("LoRa Ready");

  // WiFi
  WiFi.begin(ssid, password);
  lcd.setCursor(0, 1);
  lcd.print("WiFi..");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  lcd.setCursor(0, 1);
  lcd.print("WiFi OK        ");
}

//--- Main Loop ---
void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }

    Serial.print("LoRa: ");
    Serial.println(incoming); // Ex: "36.8,78,98"

    float temp = 0;
    int bpm = 0, spo2 = 0;

    int c1 = incoming.indexOf(',');
    int c2 = incoming.indexOf(',', c1 + 1);

    if (c1 > 0 && c2 > c1) {
      temp = incoming.substring(0, c1).toFloat();
      bpm = incoming.substring(c1 + 1, c2).toInt();
      spo2 = incoming.substring(c2 + 1).toInt();

      // Show on LCD
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("T:");
      lcd.print(temp, 1);
      lcd.print("C BPM:");
      lcd.print(bpm);

      lcd.setCursor(0, 1);
      lcd.print("SpO2:");
      lcd.print(spo2);
      lcd.print("%");

      // Send to Google Sheet
      sendToGoogleSheet(temp, bpm, spo2);
    }
  }
}

//--- HTTP to Google Sheet ---
void sendToGoogleSheet(float temp, int bpm, int spo2) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://script.google.com/macros/s/" + GAS_ID +
                 "/exec?temp=" + String(temp, 1) +
                 "&bpm=" + String(bpm) +
                 "&spo2=" + String(spo2);

    Serial.println("Sending to GAS:");
    Serial.println(url);

    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      Serial.println("Reply: " + payload);
    } else {
      Serial.println("HTTP error: " + http.errorToString(httpCode));
    }
    http.end();
  } else {
    Serial.println("WiFi Lost!");
    lcd.setCursor(0, 1);
    lcd.print("WiFi Lost     ");
  }
}
