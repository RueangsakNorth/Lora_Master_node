#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <RadioLib.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// === LCD1602 I2C ===
#define SDA_LCD 33
#define SCL_LCD 34
LiquidCrystal_I2C lcd(0x27, 16, 2);

// === SX1262 LoRa ===
#define NSS 8
#define DIO1 14
#define NRST 12
#define BUSY 13
SX1262 radio = new Module(NSS, DIO1, NRST, BUSY);
#define FREQUENCY_MHZ 433.175

// === WiFi + Google Sheets ===
const char *ssid = "@Signal-Free-WIFI";
const char *password = "";
const char *host = "script.google.com";
const int httpsPort = 443;
WiFiClientSecure client;
String GAS_ID = "AKfycby0kRQSuMC7opzQ8_jmGM5P0OI265szdfLFYwgzOyw";

// === ข้อมูลจาก LoRa ===
String rxMessage = "";
String nodeId = "Unknown";
float Temp = 0.0;
int BPM = 0;
int SpO2 = 0;

void parseMessage(const String &msg)
{
  int left = msg.indexOf('[');
  int right = msg.indexOf(']');
  if (left != -1 && right != -1 && right > left)
  {
    nodeId = msg.substring(left + 1, right);
  }
  else
  {
    nodeId = "Unknown";
  }

  int tempIndex = msg.indexOf("Temp:");
  int bpmIndex = msg.indexOf("BPM:");
  int spo2Index = msg.indexOf("SpO2:");

  if (tempIndex != -1)
    Temp = msg.substring(tempIndex + 6, msg.indexOf("°C", tempIndex)).toFloat();

  if (bpmIndex != -1)
    BPM = msg.substring(bpmIndex + 5, msg.indexOf(",", bpmIndex)).toInt();

  if (spo2Index != -1)
    SpO2 = msg.substring(spo2Index + 6, msg.indexOf("%", spo2Index)).toInt();
}

void sendToGoogleSheets(float tempVal, int bpmVal, int spo2Val)
{
  if (!client.connect(host, httpsPort))
  {
    Serial.println("❌ Google connect fail");
    lcd.setCursor(0, 1);
    lcd.print("Google Fail     ");
    return;
  }

  String url = "/macros/s/" + GAS_ID + "/exec?"
                                       "temp=" +
               String(tempVal, 1) +
               "&bpm=" + String(bpmVal) +
               "&spo2=" + String(spo2Val);

  Serial.println("🌐 URL: " + url);
  lcd.setCursor(0, 1);
  lcd.print("Sent to Sheets  ");

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ESP32LoRaRX\r\n" +
               "Connection: close\r\n\r\n");

  while (client.connected())
  {
    String line = client.readStringUntil('\n');
    if (line == "\r")
      break;
  }

  String line = client.readStringUntil('\n');
  Serial.println("📄 Reply: " + line);
  client.stop();
}

void setup()
{
  Serial.begin(115200);
  Wire.begin(SDA_LCD, SCL_LCD);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connecting");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(300);
    Serial.print(".");
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi OK");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());

  client.setInsecure(); // SSL

  if (radio.begin() != RADIOLIB_ERR_NONE)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("LoRa Init FAIL");
    while (true)
      ;
  }

  radio.setFrequency(FREQUENCY_MHZ);
  lcd.setCursor(0, 1);
  lcd.print("LoRa RX Ready   ");
  delay(1000);
}

void loop()
{
  static bool waitingShown = false;
  if (!waitingShown)
  {
    Serial.println("⏳ Waiting for LoRa packet...");
    waitingShown = true;
  }

  int state = radio.receive(rxMessage);
  if (state == RADIOLIB_ERR_NONE)
  {
    waitingShown = false;
    Serial.println("📡 Packet received!");
    Serial.println("Raw: " + rxMessage);

    parseMessage(rxMessage);

    Serial.println("📥 From: " + nodeId);
    Serial.printf("Temp = %.1f | BPM = %d | SpO2 = %d\n", Temp, BPM, SpO2);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("T:" + String(Temp, 1) + "C");
    lcd.setCursor(0, 1);
    lcd.print("B:" + String(BPM) + " S:" + String(SpO2));

    sendToGoogleSheets(Temp, BPM, SpO2);
    delay(2000); // เพิ่มความถี่ส่งได้มากขึ้น
  }

  delay(100);
}
