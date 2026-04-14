// Author: Gary Gao
// Email: g44gao@uwaterloo.ca
// Date: 2024-11-3
// Description: Header file for the GC9203 LCD low-level driver.

#ifndef GC9203_DRIVER_H
#define GC9203_DRIVER_H

#include <Arduino.h>
#include <SPI.h>

class GC9203_Driver {
public:
  // Screen dimensions (use these instead of magic numbers in your sketch)
  static constexpr int16_t WIDTH  = 128;
  static constexpr int16_t HEIGHT = 220;

  // Constructor: sck/mosi are stored for documentation; hardware SPI pins are
  // fixed by the ESP8266/ESP32 silicon (GPIO14=SCK, GPIO13=MOSI on ESP8266 HSPI).
  GC9203_Driver(int8_t sck, int8_t mosi, int8_t cs, int8_t dc, int8_t rst);

  // Initialise pins, SPI, and the controller.  Must be called before anything else.
  void begin();

  // Change display orientation (0–3).  Updates hardware immediately after begin().
  void setRotation(uint8_t r);

  // Push dirty regions of the framebuffer to the display.
  void display();

  // Fill the framebuffer with black and mark every pixel as dirty.
  void clearDisplay();

  // Fill the framebuffer with a solid 8-bit colour and mark every pixel as dirty.
  void fillScreen(uint8_t color);

  // Set a single pixel in the framebuffer.  Call display() to push changes.
  void drawPixel(int16_t x, int16_t y, uint8_t color);

  // Graphics primitives (operate on framebuffer; call display() to push changes)
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color);
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);

private:
  // ── Constants ──────────────────────────────────────────────────────────────
  // Number of bytes in the quick-skip row-dirty array (1 bit per row).
  static constexpr uint8_t  ROW_QUICK_SKIP      = (HEIGHT + 7) / 8; // 28
  // Maximum gap (in pixels) between two dirty pixels before they are flushed
  // as separate SPI windows.
  static constexpr int16_t  SEPARATION_THRESHOLD = 12;
  // The GC9203 has a 176-column address space; a 128-pixel panel is centred,
  // so column 0 of the panel maps to hardware column 24.
  static constexpr uint16_t COLUMN_OFFSET        = 24;
  // SPI clock speed (Hz).  26 MHz is typically stable on ESP8266.
  static constexpr uint32_t SPI_FREQ             = 26000000UL;

  // ── Pin storage ────────────────────────────────────────────────────────────
  // _sck/_mosi are informational only on ESP8266 (hardware SPI pins are fixed).
  int8_t  _sck, _mosi, _cs, _dc, _rst;
  uint8_t _rotation;
  bool    _initialized;

  // ── Framebuffer & LUT ──────────────────────────────────────────────────────
  // Each byte packs an 8-bit colour in bits[7:1] and a dirty flag in bit[0].
  // The 8-bit colour space is 2:3:2 R:G:B.
  uint8_t  framebuffer[HEIGHT][WIDTH];
  // Dirty-row quick-skip bitmap: bit (row % 8) of byte (row / 8) is set when
  // any pixel in that row needs to be flushed.
  uint8_t  update_row[ROW_QUICK_SKIP];
  // Lookup table: 8-bit colour index → RGB565 value ready for the controller.
  // Only even indices carry meaningful colour (odd indices have the dirty flag
  // OR'd in and are not looked up).
  uint16_t lut[256];

  // ── Private helpers ────────────────────────────────────────────────────────
  void initDisplay();
  void buildLUT();

  // Compute the Entry Mode register value for the current _rotation.
  uint16_t entryModeValue() const;

  // Set the hardware GRAM write window.  Does NOT touch Entry Mode.
  void setWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

  // ── Low-level SPI ──────────────────────────────────────────────────────────
  // Send a 16-bit index-register address (DC low).
  void writeCommand(uint16_t cmd);
  // Send a 16-bit data word (DC high) with individual CS toggle.
  // Use for register writes where each transaction is independent.
  void writeData(uint16_t data);
  // Write a register command followed immediately by its data word.
  void writeCmdData(uint16_t cmd, uint16_t data);
  // Assert CS low and set DC high for a pixel data burst.
  // Caller must call endDataBurst() when done.
  void startDataBurst();
  // Deassert CS high after a pixel data burst.
  void endDataBurst();
  // Transfer a 16-bit pixel word during an active data burst (no CS/DC changes).
  void transferPixel(uint16_t pixel);
};

// Backward-compatible screen-dimension macros for sketches that use the old names.
#define GC9203_WIDTH  (GC9203_Driver::WIDTH)
#define GC9203_HEIGHT (GC9203_Driver::HEIGHT)

#endif // GC9203_DRIVER_H