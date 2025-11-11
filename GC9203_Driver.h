#ifndef GC9203_DRIVER_H
#define GC9203_DRIVER_H

#include <Arduino.h>
#include <SPI.h>

// Screen dimensions
#define GC9203_WIDTH 128
#define GC9203_HEIGHT 220

// Internal constants for partial refresh
#define ROW_QUICK_SKIP ((GC9203_HEIGHT + 7) / 8) // 220/8 = 28
#define SEPARATION_THRESHOLD 5

class GC9203_Driver {
public:
  // Constructor: Takes the SPI and control pins
  GC9203_Driver(int8_t sck, int8_t mosi, int8_t cs, int8_t dc, int8_t rst);

  // Public API: These are the functions your main sketch will call
  void begin();
  void setRotation(uint8_t r);
  void display();
  void clearDisplay();
  void fillScreen(uint8_t color);
  void drawPixel(int16_t x, int16_t y, uint8_t color);

  // --- New Graphics Primitives ---
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color);
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);
  // (You can add drawCircle, drawChar, etc. later using the same pattern)

private:
  // Internal pin storage
  int8_t _sck, _mosi, _cs, _dc, _rst;
  uint8_t _rotation;

  // Framebuffer and color lookup table (LUT)
  uint16_t lut[256]; // 8-bit to 16-bit color lookup table
  uint8_t framebuffer[GC9203_HEIGHT][GC9203_WIDTH];
  uint8_t update_row[ROW_QUICK_SKIP];

  // Internal helper functions
  void initDisplay();
  void buildLUT();
  void updateFlag(int16_t x, int16_t y);
  void setWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

  // Low-level SPI communication
  void writeCommand(uint16_t command);
  void writeData(uint16_t data);
};

#endif // GC9203_DRIVER_H