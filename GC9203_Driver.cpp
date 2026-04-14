// Author: Gary Gao
// Email: g44gao@uwaterloo.ca
// Date: 2024-11-3
// Description: GC9203 LCD low-level driver implementation.

#include "GC9203_Driver.h"
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

GC9203_Driver::GC9203_Driver(int8_t sck, int8_t mosi, int8_t cs,
                              int8_t dc,  int8_t rst)
    : _sck(sck), _mosi(mosi), _cs(cs), _dc(dc), _rst(rst),
      _rotation(0), _initialized(false)
{}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void GC9203_Driver::begin() {
  pinMode(_cs,  OUTPUT);
  pinMode(_dc,  OUTPUT);
  pinMode(_rst, OUTPUT);

  digitalWrite(_cs,  HIGH); // CS idles high
  digitalWrite(_dc,  HIGH);

  // On ESP8266 HSPI, SCK and MOSI are fixed in silicon (GPIO14, GPIO13).
  SPI.begin();

  initDisplay();
  delay(200);

  buildLUT();
  _initialized = true;

  clearDisplay();
  display();
}

void GC9203_Driver::setRotation(uint8_t r) {
  _rotation = r % 4;
  if (_initialized) {
    SPI.beginTransaction(SPISettings(SPI_FREQ, MSBFIRST, SPI_MODE0));
    writeCmdData(0x0003, entryModeValue());
    SPI.endTransaction();
  }
}

// Flush dirty pixel segments row by row using partial refresh.
void GC9203_Driver::display() {
  SPI.beginTransaction(SPISettings(SPI_FREQ, MSBFIRST, SPI_MODE0));

  for (int16_t row = 0; row < HEIGHT; ++row) {
    if ((update_row[row / 8] & (1 << (row % 8))) == 0) continue; // row clean

    int16_t x_start = -1;
    int16_t prev_x  = -1;

    for (int16_t x = 0; x < WIDTH; ++x) {
      if (framebuffer[row][x] & 0x01) { // dirty flag (bit 0)
        if (x_start == -1) {
          x_start = x;
        } else if ((x - prev_x) > SEPARATION_THRESHOLD) {
          // Gap exceeds threshold — flush current segment and start a new one.
          setWindow(static_cast<uint16_t>(x_start),
                    static_cast<uint16_t>(row),
                    static_cast<uint16_t>(prev_x),
                    static_cast<uint16_t>(row));
          startDataBurst();
          for (int16_t i = x_start; i <= prev_x; ++i) {
            framebuffer[row][i] &= ~0x01u;
            transferPixel(lut[framebuffer[row][i]]);
          }
          endDataBurst();
          x_start = x;
        }
        prev_x = x;
      }
    }

    if (x_start != -1) { // flush trailing segment
      setWindow(static_cast<uint16_t>(x_start),
                static_cast<uint16_t>(row),
                static_cast<uint16_t>(prev_x),
                static_cast<uint16_t>(row));
      startDataBurst();
      for (int16_t i = x_start; i <= prev_x; ++i) {
        framebuffer[row][i] &= ~0x01u;
        transferPixel(lut[framebuffer[row][i]]);
      }
      endDataBurst();
    }

    update_row[row / 8] &= static_cast<uint8_t>(~(1 << (row % 8)));
  }

  SPI.endTransaction();
}

// Framebuffer byte: bits[7:1] = colour, bit[0] = dirty flag.
// 0x01 encodes black (colour=0) with the dirty flag set.
void GC9203_Driver::clearDisplay() {
  memset(framebuffer, 0x01, sizeof(framebuffer));
  memset(update_row,  0xFF, sizeof(update_row));
}

void GC9203_Driver::fillScreen(uint8_t color) {
  uint8_t fb_byte = static_cast<uint8_t>(((color << 1) & 0xFE) | 0x01);
  for (int16_t row = 0; row < HEIGHT; ++row)
    memset(framebuffer[row], fb_byte, static_cast<size_t>(WIDTH));
  memset(update_row, 0xFF, sizeof(update_row));
}

void GC9203_Driver::drawPixel(int16_t x, int16_t y, uint8_t color) {
  if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
  framebuffer[y][x] = static_cast<uint8_t>(((color << 1) & 0xFE) | 0x01);
  update_row[y / 8] |= static_cast<uint8_t>(1 << (y % 8));
}

// ─────────────────────────────────────────────────────────────────────────────
// Graphics primitives
// ─────────────────────────────────────────────────────────────────────────────

void GC9203_Driver::fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                              uint8_t color) {
  if (x >= WIDTH || y >= HEIGHT || w <= 0 || h <= 0) return;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > WIDTH)  w = static_cast<int16_t>(WIDTH  - x);
  if (y + h > HEIGHT) h = static_cast<int16_t>(HEIGHT - y);

  uint8_t fb_byte = static_cast<uint8_t>(((color << 1) & 0xFE) | 0x01);
  for (int16_t row = y; row < y + h; ++row) {
    memset(&framebuffer[row][x], fb_byte, static_cast<size_t>(w));
    update_row[row / 8] |= static_cast<uint8_t>(1 << (row % 8));
  }
}

void GC9203_Driver::drawRect(int16_t x, int16_t y, int16_t w, int16_t h,
                              uint8_t color) {
  drawLine(x,         y,         x + w - 1, y,         color); // top
  drawLine(x,         y + h - 1, x + w - 1, y + h - 1, color); // bottom
  drawLine(x,         y,         x,         y + h - 1,  color); // left
  drawLine(x + w - 1, y,         x + w - 1, y + h - 1,  color); // right
}

// Bresenham's line algorithm.
void GC9203_Driver::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                              uint8_t color) {
  int16_t dx  =  abs(x1 - x0);
  int16_t dy  = -abs(y1 - y0);
  int16_t sx  = (x0 < x1) ? 1 : -1;
  int16_t sy  = (y0 < y1) ? 1 : -1;
  int16_t err = dx + dy;

  while (true) {
    drawPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    int16_t e2 = static_cast<int16_t>(2 * err);
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Private — display initialisation
// ─────────────────────────────────────────────────────────────────────────────

void GC9203_Driver::initDisplay() {
  // Hardware reset.
  digitalWrite(_rst, HIGH); delay(5);
  digitalWrite(_rst, LOW);  delay(10);
  digitalWrite(_rst, HIGH); delay(120);

  SPI.beginTransaction(SPISettings(SPI_FREQ, MSBFIRST, SPI_MODE0));

  writeCommand(0x00FE); // vendor unlock
  writeCommand(0x00FF);

  writeCmdData(0x0001, 0x001C); // R01h Driver Output Control
  writeCmdData(0x0003, entryModeValue()); // R03h Entry Mode
  writeCmdData(0x0005, 0x0000); // R05h SPI_2data (off)
  writeCmdData(0x0008, 0x0404); // R08h Display Control 2 (porch)
  writeCmdData(0x000F, 0x0701); // R0Fh Oscillator Control

  writeCmdData(0x0010, 0x0000); // R10h Power Control 1 (exit standby)
  writeCmdData(0x0011, 0x1000); // R11h Power Control 2 (auto-boost)
  delay(120);

  writeCmdData(0x0030, 0x0000); // R30h Gate Scan Control
  writeCmdData(0x0007, 0x0013); // R07h Display Control 1 (display on)

  SPI.endTransaction();
}

// 8-bit colour format: [ R1 R0 G2 G1 G0 B1 B0 F ] (bits 7…0)
// Sub-channels (R=2 bit, G=3 bit, B=2 bit) are bit-replicated to full
// RGB565 width so the maximum input value maps to the maximum output value.
//   2→5 bit: 00→00000  01→01010  10→10101  11→11111
//   3→6 bit: 000→000000  …  111→111111
void GC9203_Driver::buildLUT() {
  for (uint16_t i = 0; i < 256; ++i) {
    const uint8_t r2 = static_cast<uint8_t>((i >> 6) & 0x03); // bits 7:6
    const uint8_t g3 = static_cast<uint8_t>((i >> 3) & 0x07); // bits 5:3
    const uint8_t b2 = static_cast<uint8_t>((i >> 1) & 0x03); // bits 2:1

    const uint16_t r5 = static_cast<uint16_t>((r2 << 3) | (r2 << 1) | (r2 >> 1));
    const uint16_t g6 = static_cast<uint16_t>((g3 << 3) | g3);
    const uint16_t b5 = static_cast<uint16_t>((b2 << 3) | (b2 << 1) | (b2 >> 1));

    lut[i] = static_cast<uint16_t>((r5 << 11) | (g6 << 5) | b5);
  }
}

// Entry Mode register value for the current rotation.
uint16_t GC9203_Driver::entryModeValue() const {
  switch (_rotation) {
    case 1:  return 0x1030; // 90°
    case 2:  return 0x1028; // 180°
    case 3:  return 0x1018; // 270°
    default: return 0x1000; // 0° portrait
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Private — GRAM window & SPI helpers
// ─────────────────────────────────────────────────────────────────────────────

// GRAM address window. Column offset centres the 128-pixel panel within the
// 176-column GC9203 address space. Caller holds the SPI transaction open.
void GC9203_Driver::setWindow(uint16_t x1, uint16_t y1,
                               uint16_t x2, uint16_t y2) {
  if (_rotation == 0 || _rotation == 2) {
    writeCmdData(0x0037, x1 + COLUMN_OFFSET); // SC
    writeCmdData(0x0036, x2 + COLUMN_OFFSET); // EC
    writeCmdData(0x0039, y1);                 // SP
    writeCmdData(0x0038, y2);                 // EP
  } else {
    writeCmdData(0x0037, y1 + COLUMN_OFFSET);
    writeCmdData(0x0036, y2 + COLUMN_OFFSET);
    writeCmdData(0x0039, x1);
    writeCmdData(0x0038, x2);
  }
  writeCommand(0x0022); // GRAM write enable
}

// DC low, CS toggled — index register address.
void GC9203_Driver::writeCommand(uint16_t cmd) {
  digitalWrite(_dc, LOW);
  digitalWrite(_cs, LOW);
  SPI.transfer16(cmd);
  digitalWrite(_cs, HIGH);
}

// DC high, CS toggled — register data.
void GC9203_Driver::writeData(uint16_t data) {
  digitalWrite(_dc, HIGH);
  digitalWrite(_cs, LOW);
  SPI.transfer16(data);
  digitalWrite(_cs, HIGH);
}

// Command + data in a single call.
void GC9203_Driver::writeCmdData(uint16_t cmd, uint16_t data) {
  writeCommand(cmd);
  writeData(data);
}

// Assert CS low and DC high; hold for the entire pixel burst.
void GC9203_Driver::startDataBurst() {
  digitalWrite(_dc, HIGH);
  digitalWrite(_cs, LOW);
}

// Deassert CS high after the pixel burst.
void GC9203_Driver::endDataBurst() {
  digitalWrite(_cs, HIGH);
}

// Transfer one RGB565 pixel; CS/DC state set by startDataBurst.
void GC9203_Driver::transferPixel(uint16_t pixel) {
  SPI.transfer16(pixel);
}
