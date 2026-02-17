/*
 * ESP8266 Graph Gallery
 *
 * TFT (ST7789 240x240) displays animated graphs
 * OLED (SSD1306 128x64) shows graph name & description
 *
 * 15 different graph types with incremental animation.
 * No WiFi/API needed — pure mathematical visualization.
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

extern "C" {
  #include "user_interface.h"
}

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

// ========== Timing ==========
#define FRAME_DELAY   7     // ms per frame (approx 2x faster than 15ms)
#define HOLD_TIME     2000  // ms to hold after completion (2 seconds)

// ========== Colors ==========
#define C_BG      0x0000
#define C_WHITE   0xFFFF
#define C_RED     0xF800
#define C_GREEN   0x07E0
#define C_BLUE    0x001F
#define C_CYAN    0x07FF
#define C_MAGENTA 0xF81F
#define C_YELLOW  0xFFE0
#define C_ORANGE  0xFD20
#define C_PURPLE  0x780F
#define C_PINK    0xFC18
#define C_DKGRAY  0x2104
#define C_LTGRAY  0x8410

// ========== Graph Entry ==========
typedef void (*InitFunc)();
typedef void (*StepFunc)(int);

struct GraphEntry {
  const char* name;
  const char* line1;
  const char* line2;
  InitFunc init;
  StepFunc step;
  int maxSteps;
};

// Forward declarations
void initSineWave();     void stepSineWave(int s);
void initBarChart();     void stepBarChart(int s);
void initPieChart();     void stepPieChart(int s);
void initLineGraph();    void stepLineGraph(int s);
void initScatterPlot();  void stepScatterPlot(int s);
void initRadarChart();   void stepRadarChart(int s);
void initAreaChart();    void stepAreaChart(int s);
void initGauge();        void stepGauge(int s);
void initHistogram();    void stepHistogram(int s);
void initSpiral();       void stepSpiral(int s);
void initLissajous();    void stepLissajous(int s);
void initRoseCurve();    void stepRoseCurve(int s);
void initHeartCurve();   void stepHeartCurve(int s);
void initVortexDots();   void stepVortexDots(int s);
void initMoireLines();   void stepMoireLines(int s);
void initSystemInfo();   void stepSystemInfo(int s);

const GraphEntry graphs[] = {
  {"Sine Wave",    "Periodic function", "y = sin(x)",        initSineWave,    stepSineWave,    210},
  {"Bar Chart",    "Category compare",  "5 data bars",       initBarChart,    stepBarChart,    65},
  {"Pie Chart",    "Proportions",       "Arc segments",      initPieChart,    stepPieChart,    40},
  {"Line Graph",   "Trend analysis",    "Multi-series",      initLineGraph,   stepLineGraph,   200},
  {"Scatter Plot", "Correlation",       "Random points",     initScatterPlot, stepScatterPlot, 55},
  {"Radar Chart",  "Multi-axis",        "Pentagon shape",    initRadarChart,  stepRadarChart,  40},
  {"Area Chart",   "Cumulative",        "Filled curve",      initAreaChart,   stepAreaChart,   105},
  {"Gauge",        "Single value",      "Needle + arc",      initGauge,       stepGauge,       60},
  {"Histogram",    "Distribution",      "Frequency bins",    initHistogram,   stepHistogram,   65},
  {"Spiral",       "Archimedes",        "r = a + b*theta",   initSpiral,      stepSpiral,      130},
  {"Lissajous",    "Frequency ratio",   "Parametric XY",     initLissajous,   stepLissajous,   200},
  {"Rose Curve",   "Polar coords",      "r = cos(7*theta)",  initRoseCurve,   stepRoseCurve,   200},
  {"Heart Curve",  "Parametric",        "Love equation",     initHeartCurve,  stepHeartCurve,  130},
  {"Vortex Dots",  "Particle swarm",    "Spiral vortex",     initVortexDots,  stepVortexDots,  150},
  {"Moire Lines",  "Interference",      "Radial lines",      initMoireLines,  stepMoireLines,  100},
  {"System Info",  "Hardware specs",    "ESP8266 stats",     initSystemInfo,  stepSystemInfo,  1},
};

const int NUM_GRAPHS = sizeof(graphs) / sizeof(graphs[0]);

int currentGraph = 0;
int currentStep = 0;
bool needsInit = true;

// ========== Pseudo-random data (seeded per graph) ==========
// Simple data arrays generated in init functions
float barData[8];
float pieData[6];
float lineData[4][50];
float scatterX[80], scatterY[80];
float radarData[5];
float areaData[110];
int histData[12];

// Starfield data
#define NUM_STARS 40
float starX[NUM_STARS], starY[NUM_STARS], starZ[NUM_STARS];

// Matrix rain data
#define MATRIX_COLS 15
int matrixY[MATRIX_COLS];
int matrixSpeed[MATRIX_COLS];
int matrixLen[MATRIX_COLS];

// ========== Helper: Ultra Fast Clear (Direct GPIO) ==========
void fastClear() {
  tft.startWrite();
  tft.setAddrWindow(0, 0, 240, 240);
  
  uint32_t sclk = (1 << TFT_SCLK);
  uint32_t mosi = (1 << TFT_MOSI);
  
  // MOSI LOW (Black = 0x0000)
  GPOC = mosi;
  
  // Total 57,600 pixels.
  // We unroll 16 bits per pixel to KILL loop overhead.
  for (uint32_t i = 0; i < 57600; i++) {
    // 16 CLK Toggles for one black pixel (Hand-unrolled)
    GPOS = sclk; GPOC = sclk; GPOS = sclk; GPOC = sclk;
    GPOS = sclk; GPOC = sclk; GPOS = sclk; GPOC = sclk;
    GPOS = sclk; GPOC = sclk; GPOS = sclk; GPOC = sclk;
    GPOS = sclk; GPOC = sclk; GPOS = sclk; GPOC = sclk;
    GPOS = sclk; GPOC = sclk; GPOS = sclk; GPOC = sclk;
    GPOS = sclk; GPOC = sclk; GPOS = sclk; GPOC = sclk;
    GPOS = sclk; GPOC = sclk; GPOS = sclk; GPOC = sclk;
    GPOS = sclk; GPOC = sclk; GPOS = sclk; GPOC = sclk;
  }
  tft.endWrite();
  yield(); // Full screen cleared, now yield once.
}

// ========== Helper: draw axes ==========
void drawAxes(int x0, int y0, int w, int h, uint16_t color) {
  tft.drawFastVLine(x0, y0 - h, h, color);
  tft.drawFastHLine(x0, y0, w, color);
  // Arrow tips
  tft.drawLine(x0 - 2, y0 - h + 5, x0, y0 - h, color);
  tft.drawLine(x0 + 2, y0 - h + 5, x0, y0 - h, color);
  tft.drawLine(x0 + w - 5, y0 - 2, x0 + w, y0, color);
  tft.drawLine(x0 + w - 5, y0 + 2, x0 + w, y0, color);
}

// ========== Helper: draw title on TFT ==========
void drawTFTTitle(const char* title) {
  // No fillRect here to avoid line-by-line wipe
  tft.setTextSize(2);
  tft.setTextColor(C_CYAN);
  tft.setCursor(8, 4);
  tft.print(title);
}

// ========== OLED display ==========
void drawOLED(int idx) {
  oled.clearDisplay();
  oled.setTextColor(WHITE);

  // Graph number
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print(idx + 1);
  oled.print(" / ");
  oled.print(NUM_GRAPHS);

  // Graph name (large)
  oled.setTextSize(2);
  oled.setCursor(0, 16);
  oled.print(graphs[idx].name);

  // Divider
  oled.drawFastHLine(0, 36, 128, WHITE);

  // Description lines
  oled.setTextSize(1);
  oled.setCursor(0, 42);
  oled.print(graphs[idx].line1);
  oled.setCursor(0, 54);
  oled.print(graphs[idx].line2);

  oled.display();
}

// ========================================================
//  GRAPH 1: Sine Wave
// ========================================================
void initSineWave() {
  drawTFTTitle("Sine Wave");
  drawAxes(15, 180, 210, 130, C_DKGRAY);
  // Y-axis labels
  tft.setTextSize(1);
  tft.setTextColor(C_LTGRAY);
  tft.setCursor(0, 47);  tft.print("1");
  tft.setCursor(0, 110); tft.print("0");
  tft.setCursor(0, 173); tft.print("-1");
  // X-axis tick marks (pi units)
  for (int i = 1; i <= 4; i++) {
    int tx = 15 + (int)(i * 210.0 / 4.0);
    tft.drawFastVLine(tx, 178, 5, C_LTGRAY);
    tft.setCursor(tx - 6, 185);
    tft.print(i);
    tft.print("p");
  }
}

void stepSineWave(int s) {
  if (s >= 210) return;
  float x = s * (4.0 * PI) / 210.0;
  int px = 16 + s;
  int py = 115 - (int)(sin(x) * 60.0);
  if (s > 0) {
    float xp = (s - 1) * (4.0 * PI) / 210.0;
    int ppx = 16 + (s - 1);
    int ppy = 115 - (int)(sin(xp) * 60.0);
    tft.drawLine(ppx, ppy, px, py, C_GREEN);
  }
  tft.fillCircle(px, py, 2, C_YELLOW);
  if (s > 1) {
    float xpp = (s - 1) * (4.0 * PI) / 210.0;
    int ppx = 16 + (s - 1);
    int ppy = 115 - (int)(sin(xpp) * 60.0);
    tft.fillCircle(ppx, ppy, 2, C_GREEN);
  }
}

// ========================================================
//  GRAPH 2: Bar Chart
// ========================================================
void initBarChart() {
  drawTFTTitle("Bar Chart");
  barData[0] = 75; barData[1] = 45; barData[2] = 90;
  barData[3] = 60; barData[4] = 85;
  drawAxes(25, 220, 200, 180, C_DKGRAY);
  // Labels
  tft.setTextSize(1);
  tft.setTextColor(C_LTGRAY);
  const char* labels[] = {"A", "B", "C", "D", "E"};
  for (int i = 0; i < 5; i++) {
    tft.setCursor(45 + i * 38, 224);
    tft.print(labels[i]);
  }
}

void stepBarChart(int s) {
  uint16_t colors[] = {C_RED, C_GREEN, C_BLUE, C_ORANGE, C_PURPLE};
  for (int i = 0; i < 5; i++) {
    float maxH = barData[i] * 1.7f;
    float curH = maxH * min((float)s / 65.0f, 1.0f);
    int barW = 30;
    int barX = 30 + i * 38;
    int barY = 220 - (int)curH;
    // Build bar upwards: draw a thin segment at the current rising top
    if (curH > 0) {
      tft.fillRect(barX, barY, barW, 2, colors[i]);
    }
  }
}

// ========================================================
//  GRAPH 3: Pie Chart
// ========================================================
void initPieChart() {
  drawTFTTitle("Pie Chart");
  pieData[0] = 30; pieData[1] = 25; pieData[2] = 20;
  pieData[3] = 15; pieData[4] = 10;
  // Draw center circle outline
  tft.drawCircle(120, 135, 81, C_DKGRAY);
}

void stepPieChart(int s) {
  uint16_t colors[] = {C_RED, C_ORANGE, C_GREEN, C_CYAN, C_PURPLE};
  float totalAngle = (float)s / 40.0f * 360.0f;
  float accumulated = 0;
  int cx = 120, cy = 135, r = 80;

  for (int i = 0; i < 5; i++) {
    float sweep = pieData[i] / 100.0f * 360.0f;
    float segStart = accumulated;
    float segEnd = accumulated + sweep;

    float drawFrom = max(totalAngle - (360.0f/40.0f), segStart);
    float drawTo = min(totalAngle, segEnd);

    if (drawFrom < drawTo) {
      for (float a = drawFrom; a <= drawTo; a += 1.5f) {
        float rad = (a - 90.0f) * PI / 180.0f;
        int ex = cx + (int)(r * cos(rad));
        int ey = cy + (int)(r * sin(rad));
        tft.drawLine(cx, cy, ex, ey, colors[i]);
      }
    }
    accumulated += sweep;
  }

  if (s == 39) {
    const char* labels[] = {"A", "B", "C", "D", "E"};
    float acc = 0;
    tft.setTextSize(2);
    for (int i = 0; i < 5; i++) {
      float sweep = pieData[i] / 100.0f * 360.0f;
      float midDeg = acc + sweep / 2.0f - 90.0f;
      float midRad = midDeg * PI / 180.0f;
      int lx = cx + (int)(50.0f * cos(midRad)) - 5;
      int ly = cy + (int)(50.0f * sin(midRad)) - 7;
      tft.setTextColor(C_WHITE); tft.setCursor(lx, ly); tft.print(labels[i]);
      acc += sweep;
    }
  }
}

// ========================================================
//  GRAPH 4: Line Graph (multi-series)
// ========================================================
void initLineGraph() {
  drawTFTTitle("Line Graph");
  randomSeed(42);
  for (int j = 0; j < 3; j++) {
    float val = 50 + random(30);
    for (int i = 0; i < 50; i++) {
      val += (random(21) - 10) * 0.5;
      val = constrain(val, 10, 100);
      lineData[j][i] = val;
    }
  }
  drawAxes(25, 220, 200, 180, C_DKGRAY);

  // Axis Labels and Tick Marks
  tft.setTextSize(1);
  tft.setTextColor(C_LTGRAY);

  // Y-axis (Value) 0, 50, 100
  for (int i = 0; i <= 100; i += 50) {
    int yPos = 220 - (int)(i * 1.7);
    tft.drawFastHLine(21, yPos, 4, C_LTGRAY);
    tft.setCursor(2, yPos - 3);
    tft.print(i);
  }

  // X-axis (Time/Index) 0, 25, 50
  for (int i = 0; i <= 50; i += 25) {
    int xPos = 26 + i * 4;
    if (xPos > 225) xPos = 225;
    tft.drawFastVLine(xPos, 220, 4, C_LTGRAY);
    tft.setCursor(xPos - 5, 226);
    tft.print(i);
  }

  // Legend
  tft.setTextColor(C_RED);    tft.setCursor(170, 30); tft.print("CPU");
  tft.setTextColor(C_GREEN);  tft.setCursor(195, 30); tft.print("MEM");
  tft.setTextColor(C_CYAN);   tft.setCursor(220, 30); tft.print("NET");
}

void stepLineGraph(int s) {
  int idx = s / 4;  // 0..49
  if (idx < 1 || idx >= 50) return;

  uint16_t colors[] = {C_RED, C_GREEN, C_CYAN};
  for (int j = 0; j < 3; j++) {
    int x1 = 26 + (idx - 1) * 4;
    int y1 = 220 - (int)(lineData[j][idx - 1] * 1.7);
    int x2 = 26 + idx * 4;
    int y2 = 220 - (int)(lineData[j][idx] * 1.7);
    tft.drawLine(x1, y1, x2, y2, colors[j]);
  }
}

// ========================================================
//  GRAPH 5: Scatter Plot
// ========================================================
void initScatterPlot() {
  drawTFTTitle("Scatter Plot");
  randomSeed(123);
  for (int i = 0; i < 80; i++) {
    float bx = random(200);
    scatterX[i] = bx;
    scatterY[i] = bx * 0.7 + (random(80) - 40);
  }
  drawAxes(25, 220, 200, 180, C_DKGRAY);
}

void stepScatterPlot(int s) {
  if (s >= 80) return;
  int px = 26 + (int)scatterX[s];
  int py = 220 - (int)constrain(scatterY[s], 0, 170);
  tft.fillCircle(px, py, 3, C_CYAN);
  tft.drawCircle(px, py, 3, C_WHITE);
}

// ========================================================
//  GRAPH 6: Radar Chart
// ========================================================
void initRadarChart() {
  drawTFTTitle("Radar Chart");
  radarData[0] = 0.9; radarData[1] = 0.6; radarData[2] = 0.8;
  radarData[3] = 0.5; radarData[4] = 0.7;

  // Draw grid
  int cx = 120, cy = 140, maxR = 80;
  for (int ring = 1; ring <= 4; ring++) {
    int r = maxR * ring / 4;
    for (int i = 0; i < 5; i++) {
      float a1 = (i * 72 - 90) * PI / 180.0;
      float a2 = ((i + 1) * 72 - 90) * PI / 180.0;
      tft.drawLine(cx + r * cos(a1), cy + r * sin(a1),
                    cx + r * cos(a2), cy + r * sin(a2), C_DKGRAY);
    }
  }
  // Axes
  for (int i = 0; i < 5; i++) {
    float a = (i * 72 - 90) * PI / 180.0;
    tft.drawLine(cx, cy, cx + maxR * cos(a), cy + maxR * sin(a), C_DKGRAY);
  }
  // Labels
  const char* labs[] = {"STR", "DEX", "INT", "WIS", "CHA"};
  tft.setTextSize(1);
  tft.setTextColor(C_LTGRAY);
  for (int i = 0; i < 5; i++) {
    float a = (i * 72 - 90) * PI / 180.0;
    tft.setCursor(cx + (maxR + 10) * cos(a) - 8, cy + (maxR + 10) * sin(a) - 4);
    tft.print(labs[i]);
  }
}

void stepRadarChart(int s) {
  int cx = 120, cy = 140, maxR = 80;
  float scale = min((float)s / 35.0f, 1.0f);

  // Erase previous polygon (if animating)
  if (s > 0 && s < 40) {
    float prevScale = (float)(s - 1) / 35.0f;
    for (int i = 0; i < 5; i++) {
      int j = (i + 1) % 5;
      float a1 = (i * 72 - 90) * PI / 180.0;
      float a2 = (j * 72 - 90) * PI / 180.0;
      tft.drawLine(cx + (int)(maxR * radarData[i] * prevScale * cos(a1)),
                    cy + (int)(maxR * radarData[i] * prevScale * sin(a1)),
                    cx + (int)(maxR * radarData[j] * prevScale * cos(a2)),
                    cy + (int)(maxR * radarData[j] * prevScale * sin(a2)), C_BG);
    }
  }

  // Draw current polygon
  for (int i = 0; i < 5; i++) {
    int j = (i + 1) % 5;
    float a1 = (i * 72 - 90) * PI / 180.0;
    float a2 = (j * 72 - 90) * PI / 180.0;
    tft.drawLine(cx + (int)(maxR * radarData[i] * scale * cos(a1)),
                  cy + (int)(maxR * radarData[i] * scale * sin(a1)),
                  cx + (int)(maxR * radarData[j] * scale * cos(a2)),
                  cy + (int)(maxR * radarData[j] * scale * sin(a2)), C_GREEN);
  }

  // Draw Vertices only at the very end
  if (s == 39) {
    for (int i = 0; i < 5; i++) {
      float a = (i * 72 - 90) * PI / 180.0;
      int x = cx + (int)(maxR * radarData[i] * cos(a));
      int y = cy + (int)(maxR * radarData[i] * sin(a));
      tft.fillCircle(x, y, 4, C_YELLOW);
    }
  }
}

// ========================================================
//  GRAPH 7: Area Chart
// ========================================================
void initAreaChart() {
  drawTFTTitle("Area Chart");
  randomSeed(77);
  float val = 40;
  // 105 points * 2px = 210px (Full width of axes)
  for (int i = 0; i < 105; i++) {
    val += (random(21) - 10) * 0.9;
    val = constrain(val, 10, 100);
    areaData[i] = val;
  }
  drawAxes(20, 225, 210, 185, C_DKGRAY);
}

void stepAreaChart(int s) {
  if (s >= 105) return;
  int x = 21 + s * 2;
  int h = (int)(areaData[s] * 1.8);
  int yTop = 225 - h;
  
  // Premium Design: Deep purple body with vibrant magenta top line
  tft.drawFastVLine(x, yTop, 225 - yTop, 0x4008);    // Deep Purple Body
  tft.drawFastVLine(x + 1, yTop, 225 - yTop, 0x4008);
  
  // Bright top line
  tft.drawPixel(x, yTop, C_MAGENTA);
  tft.drawPixel(x + 1, yTop, C_MAGENTA);
  
  // Connecting line (Smoother feeling)
  if (s > 0) {
    int pyTop = 225 - (int)(areaData[s-1] * 1.8);
    tft.drawLine(x - 2, pyTop, x, yTop, C_MAGENTA);
  }
}

// ========================================================
//  GRAPH 8: Gauge
// ========================================================
void initGauge() {
  drawTFTTitle("Gauge");
  int cx = 120, cy = 155, r = 90;

  // Draw arc background (180 degrees, bottom half)
  for (float a = PI; a <= 2 * PI; a += 0.01) {
    int x = cx + (int)(r * cos(a));
    int y = cy + (int)(r * sin(a));
    tft.drawPixel(x, y, C_DKGRAY);
    x = cx + (int)((r - 1) * cos(a));
    y = cy + (int)((r - 1) * sin(a));
    tft.drawPixel(x, y, C_DKGRAY);
  }

  // Tick marks
  tft.setTextSize(1);
  tft.setTextColor(C_LTGRAY);
  for (int i = 0; i <= 10; i++) {
    float a = PI + i * PI / 10.0;
    int x1 = cx + (int)((r - 5) * cos(a));
    int y1 = cy + (int)((r - 5) * sin(a));
    int x2 = cx + (int)((r + 5) * cos(a));
    int y2 = cy + (int)((r + 5) * sin(a));
    tft.drawLine(x1, y1, x2, y2, C_LTGRAY);
  }
  // "0" and "100" labels
  tft.setCursor(20, cy + 5);  tft.print("0");
  tft.setCursor(210, cy + 5); tft.print("100");
}

void stepGauge(int s) {
  int cx = 120, cy = 155, r = 80;
  float targetVal = 72.0;
  float prevVal = targetVal * min((float)max(0, s - 1) / 40.0f, 1.0f);
  float val = targetVal * min((float)s / 40.0f, 1.0f);
  float prevA = PI + prevVal / 100.0f * PI;
  float angle = PI + val / 100.0f * PI;

  // Only draw the NEW arc slice (incremental)
  float fromA = (s == 0) ? PI : prevA;
  for (float a = fromA; a <= angle; a += 0.01f) {
    uint16_t c = (a - PI) / PI < 0.5f ? C_GREEN :
                 (a - PI) / PI < 0.8f ? C_YELLOW : C_RED;
    for (int dr = -4; dr <= 4; dr++) {
      int x = cx + (int)((r + dr) * cos(a));
      int y = cy + (int)((r + dr) * sin(a));
      tft.drawPixel(x, y, c);
    }
  }

  // Needle (NO ERASING)
  tft.drawLine(cx, cy, cx + (int)(65 * cos(angle)), cy + (int)(65 * sin(angle)), C_WHITE);
  tft.fillCircle(cx, cy, 5, C_RED);

  // Value text
  if (s % 5 == 0) {
    tft.fillRect(85, 195, 70, 25, C_BG);
    tft.setTextSize(2);
    tft.setTextColor(C_WHITE);
    tft.setCursor(90, 200);
    tft.print((int)val);
    tft.print("%");
  }
}

// ========================================================
//  GRAPH 9: Histogram
// ========================================================
void initHistogram() {
  drawTFTTitle("Histogram");
  // Generate normal-ish distribution
  randomSeed(99);
  for (int i = 0; i < 12; i++) histData[i] = 0;
  for (int i = 0; i < 200; i++) {
    int val = (random(12) + random(12)) / 2;  // crude normal
    if (val >= 0 && val < 12) histData[val]++;
  }
  drawAxes(20, 225, 210, 190, C_DKGRAY);
  // X-axis labels
  tft.setTextSize(1);
  tft.setTextColor(C_LTGRAY);
  for (int i = 0; i < 12; i += 2) {
    tft.setCursor(26 + i * 17, 228);
    tft.print(i);
  }
  // X-axis title
  tft.setCursor(90, 236);
  tft.print("Value");
  // Y-axis labels
  tft.setCursor(0, 220);  tft.print("0");
  tft.setCursor(0, 145);  tft.print("20");
  tft.setCursor(0, 65);   tft.print("40");
  // Y-axis title (vertical)
  tft.setCursor(0, 30);   tft.print("Freq");
}

void stepHistogram(int s) {
  for (int i = 0; i < 12; i++) {
    float maxH = histData[i] * 4.0f;
    float curH = maxH * min((float)s / 65.0f, 1.0f);
    int barW = 15;
    int barX = 24 + i * 17;
    int barY = 225 - (int)curH;
    // Build histogram upwards: draw segment at rising height
    if (curH > 0) {
      uint16_t c = (i < 4) ? C_BLUE : (i < 8) ? C_CYAN : C_BLUE;
      tft.fillRect(barX, barY, barW, 2, c);
    }
  }
}

// ========================================================
//  GRAPH 10: Archimedes Spiral
// ========================================================
void initSpiral() {
  drawTFTTitle("Spiral");
}

void stepSpiral(int s) {
  float maxTheta = 6.0 * PI;
  float theta = maxTheta * s / 130.0;
  float r = 3.0 + theta * 5.5;
  int cx = 120, cy = 140;
  int x = cx + (int)(r * cos(theta));
  int y = cy + (int)(r * sin(theta));

  if (s > 0) {
    float pt = maxTheta * (s - 1) / 130.0;
    float pr = 3.0 + pt * 5.5;
    int px = cx + (int)(pr * cos(pt));
    int py = cy + (int)(pr * sin(pt));
    // Color gradient
    uint8_t hue = (s * 255 / 130);
    uint16_t c = tft.color565(hue, 255 - hue / 2, 128 + hue / 2);
    tft.drawLine(px, py, x, y, c);
  }
}

// ========================================================
//  GRAPH 11: Lissajous Curve
// ========================================================
void initLissajous() {
  drawTFTTitle("Lissajous");
  tft.setTextSize(1);
  tft.setTextColor(C_LTGRAY);
  tft.setCursor(8, 225);
  tft.print("x=sin(5t) y=sin(4t)");
}

void stepLissajous(int s) {
  float t = s * 2.0 * PI / 200.0;
  int cx = 120, cy = 132;
  int x = cx + (int)(95.0 * sin(5.0 * t));
  int y = cy + (int)(85.0 * sin(4.0 * t));

  if (s > 0) {
    float pt = (s - 1) * 2.0 * PI / 200.0;
    int px = cx + (int)(95.0 * sin(5.0 * pt));
    int py = cy + (int)(85.0 * sin(4.0 * pt));
    uint8_t g = 100 + (s * 155 / 200);
    tft.drawLine(px, py, x, y, tft.color565(0, g, 255));
  }
}

// ========================================================
//  GRAPH 12: Rose Curve
// ========================================================
void initRoseCurve() {
  drawTFTTitle("Rose Curve");
  tft.setTextSize(1);
  tft.setTextColor(C_LTGRAY);
  tft.setCursor(8, 225);
  tft.print("r = cos(7*theta)");
}

void stepRoseCurve(int s) {
  int cx = 120, cy = 135;
  float theta = s * 2.0 * PI / 200.0;
  float r = 90.0 * cos(7.0 * theta);
  int x = cx + (int)(r * cos(theta));
  int y = cy + (int)(r * sin(theta));

  if (s > 0) {
    float pt = (s - 1) * 2.0 * PI / 200.0;
    float pr = 90.0 * cos(7.0 * pt);
    int px = cx + (int)(pr * cos(pt));
    int py = cy + (int)(pr * sin(pt));
    tft.drawLine(px, py, x, y, C_MAGENTA);
  }
}

// ========================================================
//  GRAPH 13: Heart Curve
// ========================================================
void initHeartCurve() {
  drawTFTTitle("Heart Curve");
}

void stepHeartCurve(int s) {
  int cx = 120, cy = 130;
  float t = s * 2.0 * PI / 130.0;
  float scale = 6.5;
  // Heart parametric: x = 16sin^3(t), y = 13cos(t) - 5cos(2t) - 2cos(3t) - cos(4t)
  float hx = 16.0 * pow(sin(t), 3);
  float hy = -(13.0 * cos(t) - 5.0 * cos(2 * t) - 2.0 * cos(3 * t) - cos(4 * t));
  int x = cx + (int)(hx * scale);
  int y = cy + (int)(hy * scale);

  if (s > 0) {
    float pt = (s - 1) * 2.0 * PI / 130.0;
    float phx = 16.0 * pow(sin(pt), 3);
    float phy = -(13.0 * cos(pt) - 5.0 * cos(2 * pt) - 2.0 * cos(3 * pt) - cos(4 * pt));
    int px = cx + (int)(phx * scale);
    int py = cy + (int)(phy * scale);
    tft.drawLine(px, py, x, y, C_RED);
  }

  // Fill effect: draw line from center to point
  if (s > 50 && s % 3 == 0) {
    tft.drawLine(cx, cy + 10, x, y, 0x8000);  // dark red fill
  }
}

// ========================================================
//  GRAPH 14: Vortex Dots
// ========================================================
void initVortexDots() {
  drawTFTTitle("Vortex Dots");
}

void stepVortexDots(int s) {
  int cx = 120, cy = 135;
  for (int i = 0; i < 8; i++) {
    float angle = (s * 5.0 + i * 45.0) * PI / 180.0;
    float r = 10.0 + s * 0.8;
    int x1 = cx + (int)(r * cos(angle));
    int y1 = cy + (int)(r * sin(angle));
    int x2 = cx + (int)((r+5) * cos(angle + 0.2));
    int y2 = cy + (int)((r+5) * sin(angle + 0.2));
    
    tft.drawPixel(x1, y1, C_CYAN);
    tft.drawPixel(x2, y2, C_MAGENTA);
    tft.drawPixel(x1+1, y1+1, C_WHITE);
  }
}

// ========================================================
//  GRAPH 15: Moiré Lines
// ========================================================
void initMoireLines() {
  drawTFTTitle("Moire Lines");
}

void stepMoireLines(int s) {
  int cx = 120, cy = 135;
  float angle = s * 3.6 * PI / 180.0;
  int x = cx + (int)(110 * cos(angle));
  int y = cy + (int)(110 * sin(angle));
  
  uint16_t color = tft.color565(s * 2, 255 - s * 2, 255);
  tft.drawLine(cx, cy, x, y, color);
  
  // Opposite side
  int ox = cx - (int)(110 * cos(angle));
  int oy = cy - (int)(110 * sin(angle));
  tft.drawLine(cx, cy, ox, oy, tft.color565(255, s * 2, s));
}

// ========================================================
//  GRAPH 16: System Info
// ========================================================
void initSystemInfo() {
  drawTFTTitle("System Info");
}

void stepSystemInfo(int s) {
  if (s > 0) return;
  tft.setTextSize(2);
  int y = 55; int gap = 32;

  tft.setTextColor(C_WHITE);  tft.setCursor(15, y); tft.print("CPU:");
  tft.setTextColor(C_GREEN);  tft.setCursor(110, y); tft.print("160 MHz"); y += gap;

  tft.setTextColor(C_WHITE);  tft.setCursor(15, y); tft.print("Heap:");
  tft.setTextColor(C_YELLOW); tft.setCursor(110, y); tft.print(ESP.getFreeHeap()/1024); tft.print(" KB"); y += gap;

  tft.setTextColor(C_WHITE);  tft.setCursor(15, y); tft.print("Flash:");
  tft.setTextColor(C_CYAN);   tft.setCursor(110, y); tft.print(ESP.getFlashChipRealSize()/1024/1024); tft.print(" MB"); y += gap;

  tft.setTextColor(C_WHITE);  tft.setCursor(15, y); tft.print("ChipID:");
  tft.setTextColor(C_PINK);   tft.setCursor(110, y); tft.print(ESP.getChipId(), HEX); y += gap;

  tft.setTextColor(C_WHITE);  tft.setCursor(15, y); tft.print("SDK:");
  tft.setTextSize(1);
  tft.setTextColor(C_LTGRAY); tft.setCursor(110, y + 5); tft.print(ESP.getSdkVersion());

  tft.drawRect(8, 45, 224, 180, C_DKGRAY);
}

// ========== Setup ==========
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Graph Gallery Starting ===");

  // BOOST CPU SPEED from 80MHz to 160MHz
  system_update_cpu_freq(SYS_CPU_160MHZ);
  Serial.println("CPU Clock: 160MHz (Turbo Mode)");
  tft.init(240, 240);
  tft.setRotation(2);
  tft.fillScreen(C_BG);
  tft.fillScreen(C_BG);
  // Beautiful Highlighted Start Page
  tft.setTextSize(4);
  tft.setTextColor(C_CYAN);
  tft.setCursor(25, 60);
  tft.print("GRAPH");
  tft.setTextColor(C_MAGENTA);
  tft.setCursor(25, 110);
  tft.print("GALLERY");
  
  tft.drawFastHLine(25, 155, 190, C_WHITE);
  
  tft.setTextSize(2);
  tft.setTextColor(C_YELLOW);
  tft.setCursor(25, 175);
  tft.print("v3.0 Turbo");

  // OLED init
  Wire.begin(OLED_SDA, OLED_SCL);
  delay(100);
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setCursor(0, 0);
  oled.println("Graph Gallery");
  oled.println("16 graphs total");
  oled.println("");
  oled.println("System: 160MHz");
  oled.display();

  randomSeed(analogRead(A0));
  delay(2000);

  currentGraph = 0;
  currentStep = 0;
  needsInit = true;
}

// ========== Loop ==========
void loop() {
  yield();

  if (needsInit) {
    fastClear(); // Instant erase
    graphs[currentGraph].init();
    drawOLED(currentGraph);
    currentStep = 0;
    needsInit = false;
  }

  if (currentStep < graphs[currentGraph].maxSteps) {
    graphs[currentGraph].step(currentStep);
    currentStep++;
    delay(FRAME_DELAY); // 2x faster animation
  } else {
    // 3. Hold completed graph ("완성 후 유지")
    if (currentGraph == NUM_GRAPHS - 1) {
      delay(7000); // 7s for last graph (System Info)
    } else {
      delay(2000); // 2s for others
    }
    currentGraph = (currentGraph + 1) % NUM_GRAPHS;
    needsInit = true;
  }
}
