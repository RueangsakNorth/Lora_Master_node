// ✅ LoRa RX + ส่งค่าอุณหภูมิ (Temp) ไปยัง Google Sheets + แสดงบน LCD1602
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <RadioLib.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define SDA_LCD 33
#define SCL_LCD 34
LiquidCrystal_I2C lcd(0x27, 16, 2);

#define NSS   8
#define DIO1  14
#define NRST  12
#define BUSY  13
SX1262 radio = new Module(NSS, DIO1, NRST, BUSY);

#define FREQUENCY_MHZ 433.175

const char* ssid = "@Signal-Free-WIFI";
const char* password = "";
const char* host = "script.google.com";
const int httpsPort = 443;
WiFiClientSecure client;
String GAS_ID = "AKfycbz6El5QqOwxaGZiGxxueb1OORSVKJBEWKZxOi7YoxT-2pzL0n9u49Q2sKMwaFo27jKI";

String rxMessage = "";
String nodeId = "Unknown";
String content = "";
float temp = 0.0;

void parseMessage(const String& msg) {
  int left = msg.indexOf('[');
  int right = msg.indexOf(']');
  if (left != -1 && right != -1 && right > left) {
    nodeId = msg.substring(left + 1, right);
    content = msg.substring(right + 2);
  } else {
    nodeId = "Unknown";
    content = msg;
  }

  int tempIndex = content.indexOf("Temp:");
  if (tempIndex != -1) {
    temp = content.substring(tempIndex + 6, content.indexOf("C")).toFloat();
  }
}

void sendToGoogleSheets(float tempVal) {
  if (!client.connect(host, httpsPort)) {
    Serial.println("❌ Connection to Google failed");
    lcd.setCursor(0, 1);
    lcd.print("Google Fail     ");
    return;
  }

  // 🔁 เลือก GAS_ID ตาม Node ID
  if (nodeId == "Node1") {
    GAS_ID = "AKfycby0kRQSuMC7opzQ8_jmGM5P0OI265szdfLFYwgzOyw";
  } else if (nodeId == "Node2") {
    GAS_ID = "AKfycby2bLSPvEkkqxzsxymMRWfS_YOGJdWqcS1bwHG6DvU";
  }

  String url = "/macros/s/" + GAS_ID + "/exec?temp=" + String(tempVal);
  Serial.println("🌐 URL: " + url);
  lcd.setCursor(0, 1);
  lcd.print("Sent to Sheets  ");

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ESP32LoRaRX\r\n" +
               "Connection: close\r\n\r\n");

  Serial.println("✅ Data sent to Google Sheets");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }
  String line = client.readStringUntil('\n');
  Serial.println("📄 Reply: " + line);
  client.stop();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_LCD, SCL_LCD);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connecting");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi OK");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());

  client.setInsecure();

  if (radio.begin() != RADIOLIB_ERR_NONE) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("LoRa Init FAIL");
    while (true);
  }

  radio.setFrequency(FREQUENCY_MHZ);
  lcd.setCursor(0, 1);
  lcd.print("LoRa RX Ready   ");
  delay(1000);
}

void loop() {
  static bool waitingShown = false;
  if (!waitingShown) {
    Serial.println("⏳ Waiting for LoRa packet...");
    waitingShown = true;
  }

  int state = radio.receive(rxMessage);
  if (state == RADIOLIB_ERR_NONE) {
    waitingShown = false;
    Serial.println("📡 Packet received!");
    Serial.println("Raw: " + rxMessage);

    parseMessage(rxMessage);

    Serial.println("📥 From: " + nodeId);
    Serial.println("Data: " + content);
    Serial.printf("Temp = %.1f\n", temp);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(nodeId + ": T" + String(temp, 1) + "C");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());

    sendToGoogleSheets(temp);
    delay(3000);
  }

  delay(100);
}
