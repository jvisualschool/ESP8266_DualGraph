#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "config.h"

// ========== OLED SSD1306 (I2C) ==========
#define OLED_SDA 14
#define OLED_SCL 12
Adafruit_SSD1306 oled(128, 64, &Wire, -1);

// ========== TFT ST7789 (Software SPI) ==========
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST   0
#define TFT_SCLK  5
#define TFT_MOSI  4
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// ========== Mode ==========
#define MODE_WEATHER 0
#define MODE_SYSTEM  1
int currentMode = MODE_WEATHER;
unsigned long lastModeSwitch = 0;
const unsigned long MODE_INTERVAL = 30000;
bool needsRedraw = true;

// ========== Weather Data ==========
struct WeatherData {
  float temp;
  float feels_like;
  int humidity;
  float wind_speed;
  int pressure;
  String description;
  String icon;
  bool valid;
} weather = {0, 0, 0, 0, 0, "", "", false};

unsigned long lastWeatherUpdate = 0;
const unsigned long WEATHER_INTERVAL = 600000;

// ========== Weather Icon ==========
String getWeatherSymbol(String icon) {
  if (icon.startsWith("01")) return "Clear";
  if (icon.startsWith("02")) return "FewCloud";
  if (icon.startsWith("03")) return "Cloud";
  if (icon.startsWith("04")) return "Overcast";
  if (icon.startsWith("09")) return "Drizzle";
  if (icon.startsWith("10")) return "Rain";
  if (icon.startsWith("11")) return "Thunder";
  if (icon.startsWith("13")) return "Snow";
  if (icon.startsWith("50")) return "Mist";
  return "?";
}

// ========== ESP Uptime String ==========
String getUptime() {
  unsigned long sec = millis() / 1000;
  unsigned long d = sec / 86400;
  unsigned long h = (sec % 86400) / 3600;
  unsigned long m = (sec % 3600) / 60;
  unsigned long s = sec % 60;
  if (d > 0) return String(d) + "d " + String(h) + "h " + String(m) + "m";
  if (h > 0) return String(h) + "h " + String(m) + "m " + String(s) + "s";
  return String(m) + "m " + String(s) + "s";
}

// ========== Draw Gauge Bar ==========
void drawGaugeBar(int x, int y, int w, int h, float percent, uint16_t color) {
  tft.fillRect(x, y, w, h, 0x2104);
  int fillW = (int)(w * percent / 100.0);
  if (fillW > w) fillW = w;
  uint16_t fillColor = color;
  if (percent > 80) fillColor = ST77XX_RED;
  else if (percent > 60) fillColor = ST77XX_YELLOW;
  tft.fillRect(x, y, fillW, h, fillColor);
  tft.drawRect(x, y, w, h, ST77XX_WHITE);
}

// ========== Draw Weather TFT ==========
void drawWeatherTFT() {
  tft.fillScreen(ST77XX_BLACK);

  if (!weather.valid) {
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_YELLOW);
    tft.setCursor(20, 100);
    tft.println("Loading...");
    return;
  }

  // Title bar
  tft.fillRect(0, 0, 240, 30, 0x000F);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(8, 7);
  tft.print("Seoul Weather");

  // Weather symbol
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(8, 42);
  tft.print(getWeatherSymbol(weather.icon));

  // Temperature
  tft.setTextSize(4);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(8, 68);
  tft.print(String(weather.temp, 1));
  tft.setTextSize(2);
  tft.print("'C");

  // Description
  tft.setTextSize(2);
  tft.setTextColor(0xBDF7);
  tft.setCursor(8, 108);
  tft.print(weather.description);

  // Divider
  tft.drawFastHLine(0, 134, 240, 0x4208);

  // Details
  tft.setTextSize(2);

  tft.setTextColor(ST77XX_ORANGE);
  tft.setCursor(8, 142);
  tft.print("Feel ");
  tft.setTextColor(ST77XX_WHITE);
  tft.print(String(weather.feels_like, 1) + "'C");

  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(8, 166);
  tft.print("Hum  ");
  tft.setTextColor(ST77XX_WHITE);
  tft.print(String(weather.humidity) + "%");

  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(8, 190);
  tft.print("Wind ");
  tft.setTextColor(ST77XX_WHITE);
  tft.print(String(weather.wind_speed, 1) + "m/s");

  tft.setTextColor(ST77XX_MAGENTA);
  tft.setCursor(8, 214);
  tft.print("hPa  ");
  tft.setTextColor(ST77XX_WHITE);
  tft.print(String(weather.pressure));
}

// ========== Draw Weather OLED ==========
void drawWeatherOLED() {
  oled.clearDisplay();
  oled.setTextColor(WHITE);

  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  char timeBuf[16];
  sprintf(timeBuf, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);

  oled.setTextSize(2);
  oled.setCursor(16, 0);
  oled.println(timeBuf);

  char dateBuf[16];
  sprintf(dateBuf, "%04d-%02d-%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
  oled.setTextSize(1);
  oled.setCursor(28, 20);
  oled.println(dateBuf);

  oled.drawFastHLine(0, 32, 128, WHITE);

  oled.setCursor(0, 38);
  oled.print("WiFi: ");
  oled.print(WiFi.RSSI());
  oled.println("dBm");

  oled.setCursor(0, 48);
  oled.print("IP:");
  oled.println(WiFi.localIP().toString());

  if (weather.valid) {
    oled.setCursor(0, 58);
    oled.print("Upd: ");
    unsigned long ago = (millis() - lastWeatherUpdate) / 1000;
    oled.print(ago);
    oled.print("s ago");
  }

  oled.display();
}

// ========== Draw System TFT (ESP8266 Info) ==========
void drawSystemTFT() {
  tft.fillScreen(0x0000);  // 순수 검정

  // Title bar - 아주 어두운 배경
  tft.fillRect(0, 0, 240, 32, 0x2104);  // 짙은 회색
  tft.setTextSize(2);
  tft.setTextColor(0xFFFF);  // 흰색
  tft.setCursor(16, 8);
  tft.print("ESP8266 Info");

  int y = 42;

  // Chip ID
  tft.setTextSize(2);
  tft.setTextColor(0x867F);  // 밝은 시안
  tft.setCursor(8, y);
  tft.print("ID  ");
  tft.setTextColor(0xFFFF);
  tft.print(String(ESP.getChipId(), HEX));

  // CPU Frequency
  y += 28;
  tft.setTextColor(0xFFE0);  // 밝은 노랑
  tft.setCursor(8, y);
  tft.print("CPU ");
  tft.setTextColor(0xFFFF);
  tft.print(String(ESP.getCpuFreqMHz()) + " MHz");

  // Flash Size
  y += 28;
  tft.setTextColor(0xFBE0);  // 밝은 주황
  tft.setCursor(8, y);
  tft.print("ROM ");
  tft.setTextColor(0xFFFF);
  tft.print(String(ESP.getFlashChipSize() / 1024) + " KB");

  // Divider
  y += 26;
  tft.drawFastHLine(8, y, 224, 0x2945);
  y += 6;

  // Free Heap - 게이지 크게
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t totalHeap = 81920;
  float heapPercent = (float)(totalHeap - freeHeap) / totalHeap * 100.0;
  tft.setTextSize(2);
  tft.setTextColor(0x07E0);
  tft.setCursor(8, y);
  tft.print("HEAP ");
  tft.setTextColor(0xFFFF);
  tft.print(String(freeHeap / 1024) + "KB");
  y += 22;
  drawGaugeBar(8, y, 200, 16, heapPercent, 0x07E0);
  tft.setTextSize(2);
  tft.setTextColor(0xFFFF);
  tft.setCursor(8, y + 20);
  tft.print(String((int)heapPercent) + "% used");

  // WiFi Signal - 게이지 크게
  y += 44;
  int32_t rssi = WiFi.RSSI();
  int signalPercent = min(max(2 * (rssi + 100), (int32_t)0), (int32_t)100);
  tft.setTextSize(2);
  tft.setTextColor(0x867F);
  tft.setCursor(8, y);
  tft.print("WiFi ");
  tft.setTextColor(0xFFFF);
  tft.print(String(rssi) + "dBm");
  y += 22;
  drawGaugeBar(8, y, 200, 16, signalPercent, 0x867F);

  // Uptime
  y += 24;
  tft.setTextSize(2);
  tft.setTextColor(0xFBE0);
  tft.setCursor(8, y);
  tft.print("UP  ");
  tft.setTextColor(0xFFFF);
  tft.print(getUptime());
}

// ========== Draw System OLED ==========
void drawSystemOLED() {
  oled.clearDisplay();
  oled.setTextColor(WHITE);

  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print("SSID:");
  oled.println(WiFi.SSID());

  oled.setCursor(0, 12);
  oled.print("IP: ");
  oled.println(WiFi.localIP().toString());

  oled.setCursor(0, 24);
  oled.print("MAC:");
  oled.println(WiFi.macAddress());

  oled.drawFastHLine(0, 35, 128, WHITE);

  oled.setCursor(0, 40);
  oled.print("Heap: ");
  oled.print(ESP.getFreeHeap() / 1024);
  oled.println("KB free");

  oled.setCursor(0, 52);
  oled.print("Up: ");
  oled.print(getUptime());

  oled.display();
}

// ========== Fetch Weather ==========
void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClient client;
  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/weather?q="
               + String(OWM_CITY) + "&appid=" + String(OWM_API_KEY)
               + "&units=metric";

  http.begin(client, url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (!err) {
      weather.temp = doc["main"]["temp"];
      weather.feels_like = doc["main"]["feels_like"];
      weather.humidity = doc["main"]["humidity"];
      weather.pressure = doc["main"]["pressure"];
      weather.wind_speed = doc["wind"]["speed"];
      weather.description = doc["weather"][0]["description"].as<String>();
      weather.icon = doc["weather"][0]["icon"].as<String>();
      weather.valid = true;
      lastWeatherUpdate = millis();
    }
  }
  http.end();
}

// ========== Setup ==========
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Dual Monitor Starting ===");

  // TFT init
  tft.init(240, 240);
  tft.setRotation(2);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(20, 60);
  tft.println("Connecting");
  tft.setCursor(20, 85);
  tft.println("WiFi...");

  // OLED init
  Wire.begin(OLED_SDA, OLED_SCL);
  delay(100);
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setCursor(0, 0);
  oled.println("Connecting WiFi...");
  oled.display();

  // WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 30) {
    delay(500);
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi OK: " + WiFi.localIP().toString());

    tft.fillScreen(ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_GREEN);
    tft.setCursor(20, 60);
    tft.println("WiFi OK!");
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(20, 90);
    tft.println("IP: " + WiFi.localIP().toString());

    oled.clearDisplay();
    oled.setCursor(0, 0);
    oled.println("WiFi Connected!");
    oled.println(WiFi.localIP().toString());
    oled.display();
  } else {
    Serial.println("WiFi FAILED");
    tft.fillScreen(ST77XX_RED);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(20, 100);
    tft.println("WiFi FAIL!");
    return;
  }

  // NTP
  configTime(9 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  delay(2000);

  // First weather fetch
  delay(1000);
  fetchWeather();

  lastModeSwitch = millis();
}

// ========== Loop ==========
void loop() {
  yield();
  unsigned long now = millis();

  // Mode rotation (30초)
  if (now - lastModeSwitch >= MODE_INTERVAL) {
    currentMode = (currentMode == MODE_WEATHER) ? MODE_SYSTEM : MODE_WEATHER;
    lastModeSwitch = now;
    needsRedraw = true;
  }

  // Weather update (10분)
  if (now - lastWeatherUpdate >= WEATHER_INTERVAL) {
    fetchWeather();
    if (currentMode == MODE_WEATHER) needsRedraw = true;
  }

  // TFT: 모드 전환 시에만 갱신
  if (needsRedraw) {
    if (currentMode == MODE_WEATHER) {
      drawWeatherTFT();
      drawWeatherOLED();
    } else {
      drawSystemTFT();
      drawSystemOLED();
    }
    needsRedraw = false;
  }

  // OLED: 1초마다 갱신 (시계)
  static unsigned long lastOledUpdate = 0;
  if (now - lastOledUpdate >= 1000) {
    if (currentMode == MODE_WEATHER) {
      drawWeatherOLED();
    }
    lastOledUpdate = now;
  }

  delay(10);
}
