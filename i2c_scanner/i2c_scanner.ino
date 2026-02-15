#include <Wire.h>

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nI2C Scanner");

  // 기본 핀 (D5=GPIO14, D6=GPIO12) 시도
  Serial.println("=== SDA=14(D5), SCL=12(D6) ===");
  Wire.begin(14, 12);
  scanI2C();

  // 반대로 시도
  Serial.println("=== SDA=12(D6), SCL=14(D5) ===");
  Wire.begin(12, 14);
  scanI2C();

  // 일반적인 I2C 핀 시도
  Serial.println("=== SDA=4(D2), SCL=5(D1) ===");
  Wire.begin(4, 5);
  scanI2C();
}

void scanI2C() {
  byte count = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("  Found: 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
      count++;
    }
  }
  if (count == 0) Serial.println("  No device found");
  Serial.println();
}

void loop() {}
