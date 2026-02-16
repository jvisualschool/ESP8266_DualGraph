# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP8266 NodeMCU v2 dual-display projects. Drives two displays simultaneously:
- **TFT** (ST7789, 240x240, Software SPI) — main content display
- **OLED** (SSD1306, 128x64, I2C) — built into the board

Two separate sketches:

### 1. Weather Monitor (`dual_monitor/`)
Alternates between two modes every 30 seconds:
- **Weather mode** — Seoul weather from OpenWeatherMap API (TFT) + clock/WiFi info (OLED)
- **System mode** — ESP8266 chip/heap/WiFi stats with gauge bars (TFT) + network details (OLED)

### 2. Graph Gallery (`graph_gallery/`)
15 animated graph types cycling automatically (no WiFi needed):
- Sine Wave, Bar Chart, Pie Chart, Line Graph, Scatter Plot, Radar Chart, Area Chart, Gauge, Histogram, Spiral, Lissajous, Rose Curve, Heart Curve, Starfield, Matrix Rain
- Incremental drawing strategy (no full-screen clear per frame)
- TFT shows animated graph, OLED shows graph name/description

## Build & Upload

Requires `arduino-cli` with ESP8266 board package and libraries installed:

```bash
# One-time setup
arduino-cli config add board_manager.additional_urls \
  https://arduino.esp8266.com/stable/package_esp8266com_index.json
arduino-cli core install esp8266:esp8266
arduino-cli lib install "Adafruit SSD1306" "Adafruit ST7735 and ST7789 Library" \
  "Adafruit GFX Library" "ArduinoJson"

# Compile (weather monitor)
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 dual_monitor/

# Compile (graph gallery — no WiFi/ArduinoJson needed)
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 graph_gallery/

# Upload (check port with: arduino-cli board list)
arduino-cli upload --fqbn esp8266:esp8266:nodemcuv2 --port /dev/cu.usbserial-10 graph_gallery/
```

## Architecture

### Pin Conflict & Software SPI

Hardware SPI SCLK (GPIO14/D5) collides with the board's built-in OLED I2C SDA pin. Therefore:
- OLED uses **hardware I2C**: D5 (SDA, GPIO14), D6 (SCL, GPIO12), address 0x3C
- TFT uses **software SPI**: D1 (CLK), D2 (MOSI), D3 (RST), D4 (DC), D8 (CS)

TFT must be initialized **before** OLED to avoid SPI/I2C bus conflicts.

### Screen Refresh Strategy

- **TFT**: Full redraw only on mode switch or new weather data (Software SPI is slow; avoids flicker)
- **OLED**: Redraws every 1 second in weather mode (clock update); I2C is fast enough
- **Weather API**: Polled every 10 minutes (`WEATHER_INTERVAL = 600000`)

### Key Files

| File | Purpose |
|------|---------|
| `dual_monitor/dual_monitor.ino` | Weather monitor firmware |
| `dual_monitor/config.h` | WiFi credentials & API key (gitignored, contains secrets) |
| `dual_monitor/config.example.h` | Template for config.h |
| `graph_gallery/graph_gallery.ino` | Graph gallery firmware — 15 animated graphs (no WiFi) |
| `dual_display_test/dual_display_test.ino` | Hardware test — verifies both displays work |
| `i2c_scanner/i2c_scanner.ino` | Debug utility — scans I2C bus for devices |
| `preview.html` | Browser-based simulation of both projects (GitHub Pages) |

### Configuration

All tunable constants are at the top of `dual_monitor.ino`:
- `MODE_INTERVAL` (30000ms) — mode switch timing
- `WEATHER_INTERVAL` (600000ms) — API poll interval
- `tft.setRotation(2)` — TFT orientation

## Important Notes

- `config.h` is in `.gitignore` — copy `config.example.h` to `config.h` and fill in credentials
- NTP is configured for KST (UTC+9): `configTime(9 * 3600, 0, ...)`
- The `WeatherData` struct holds cached API response; `weather.valid` gates display rendering
- `preview.html` is a standalone HTML file with no build step — simulates both projects (monitor + graph gallery) with live animations
- Graph gallery uses incremental drawing: each frame adds a few lines/points instead of clearing the whole screen (Software SPI optimization)
- Graph gallery `GraphEntry` struct holds function pointers for init/step + maxSteps per graph
