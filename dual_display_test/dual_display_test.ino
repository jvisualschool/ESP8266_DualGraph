#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

// === OLED SSD1306 (I2C 내장) ===
#define OLED_SDA 14  // D5
#define OLED_SCL 12  // D6
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

// === TFT ST7789 (소프트웨어 SPI) ===
#define TFT_CS   15  // D8
#define TFT_DC    2  // D4
#define TFT_RST   0  // D3
#define TFT_SCLK  5  // D1
#define TFT_MOSI  4  // D2
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nDual Display Test Start");

  // TFT 먼저 초기화 (SPI 핀이 I2C와 충돌 방지)
  Serial.println("TFT init...");
  tft.init(240, 240);
  tft.setRotation(2);
  tft.fillScreen(ST77XX_BLACK);
  Serial.println("TFT OK");

  // TFT 테스트 화면
  tft.fillScreen(ST77XX_RED);
  delay(300);
  tft.fillScreen(ST77XX_GREEN);
  delay(300);
  tft.fillScreen(ST77XX_BLUE);
  delay(300);
  tft.fillScreen(ST77XX_BLACK);

  tft.setTextSize(3);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(30, 80);
  tft.println("TFT OK!");

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(20, 130);
  tft.println("ST7789");
  tft.setCursor(20, 155);
  tft.println("240x240");

  // OLED 초기화 (TFT 이후)
  Serial.println("OLED init...");
  Wire.begin(OLED_SDA, OLED_SCL);
  delay(100);

  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED failed!");
  } else {
    Serial.println("OLED OK");
    oled.clearDisplay();
    oled.setTextSize(2);
    oled.setTextColor(WHITE);
    oled.setCursor(0, 0);
    oled.println("OLED OK!");
    oled.setTextSize(1);
    oled.setCursor(0, 24);
    oled.println("SSD1306 128x64");
    oled.setCursor(0, 36);
    oled.println("I2C 0x3C");
    oled.display();
  }

  Serial.println("All done!");
}

void loop() {
}
