#include "GC9203_Driver.h"

// Constructor
GC9203_Driver::GC9203_Driver(int8_t sck, int8_t mosi, int8_t cs, int8_t dc, int8_t rst)
    : _sck(sck), _mosi(mosi), _cs(cs), _dc(dc), _rst(rst), _rotation(0) {}

// Initialize the display (pins, SPI, and controller)
void GC9203_Driver::begin() {
  pinMode(_cs, OUTPUT);
  pinMode(_dc, OUTPUT);
  pinMode(_rst, OUTPUT);
  
  // Use hardware SPI pins
  // Note: SPI.begin() with no args uses default HSPI pins on ESP
  // On ESP8266, this is GPIO14 (SCK) and GPIO13 (MOSI).
  // Your .ino used 14 and 13, so this is correct.
  SPI.begin(); 
  // SPI.setClockDivider(SPI_CLOCK_DIV2); // SPI_CLOCK_DIV2 is very fast, let's use a defined frequency
  SPI.setFrequency(40000000); // 40 MHz is often a stable fast speed

  initDisplay();
  delay(200);
  buildLUT();
  delay(100);
  
  clearDisplay(); // Clear framebuffer
  display();      // Push empty buffer to screen
}

// Set display rotation (0-3)
void GC9203_Driver::setRotation(uint8_t r) {
  _rotation = r % 4; // Ensure 0-3
}

// --- Private Low-Level Functions ---

// Send a 16-bit command to the display
void GC9203_Driver::writeCommand(uint16_t command) {
  digitalWrite(_dc, LOW);
  digitalWrite(_cs, LOW);
  SPI.transfer16(command);
  digitalWrite(_cs, HIGH);
}

// Send 16-bit data to the display
void GC9203_Driver::writeData(uint16_t data) {
  digitalWrite(_dc, HIGH);
  digitalWrite(_cs, LOW);
  SPI.transfer16(data);
  digitalWrite(_cs, HIGH);
}

// The magic initialization sequence from your .ino file
void GC9203_Driver::initDisplay() {
  digitalWrite(_rst, HIGH);
  delay(5);
  digitalWrite(_rst, LOW);
  delay(10); 
  digitalWrite(_rst, HIGH);
  delay(120); 

  writeCommand(0xFE); // Enable advanced registers
  writeCommand(0xFF);

  writeCommand(0x0001); // Driver Output Control
  writeData(0x001C); 

  writeCommand(0x0003); // Entry Mode
  switch (_rotation) {
    case 0: writeData(0x1000); break; // 0 degrees
    case 1: writeData(0x1030); break; // 90 degrees
    case 2: writeData(0x1028); break; // 180 degrees
    case 3: writeData(0x1018); break; // 270 degrees
  }

  writeCommand(0x0005); // Spi_2data
  writeData(0x0000); 

  writeCommand(0x0008); // Display Control 2
  writeData(0x0404); 

  writeCommand(0x000F); // Oscillator Control
  writeData(0x0701); 

  // Power On Sequence
  writeCommand(0x0010); // Power Control 1
  writeData(0x0000); // Exit standby
  writeCommand(0x0011); // Power Control 2
  writeData(0x1000); // Start auto boost
  delay(120);

  writeCommand(0x0030); // Gate Scan Control
  writeData(0x0000); 

  // Final display on
  writeCommand(0x0007); // Display Control 1
  writeData(0x0013); 
}

// Build the 8-bit to 16-bit (RGB565) color lookup table
void GC9203_Driver::buildLUT() {
  for (int i = 0; i < 256; i++) {
    uint16_t b5 = 0, g6 = 0, r5 = 0;

    // This is a 2:3:2 R:G:B color mapping
    // R (2 bits)
    b5 |= (i & 0b10000000) ? 0b1100000000000000 : 0;
    b5 |= (i & 0b01000000) ? 0b0011000000000000 : 0;
    // G (3 bits)
    g6 |= (i & 0b00100000) ? 0b0000011000000000 : 0;
    g6 |= (i & 0b00010000) ? 0b0000000110000000 : 0;
    g6 |= (i & 0b00001000) ? 0b0000000001100000 : 0;
    // B (2 bits)
    r5 |= (i & 0b00000100) ? 0b0000000000011000 : 0;
    r5 |= (i & 0b00000010) ? 0b0000000000000110 : 0;
    // Bit 0 (0b00000001) is reserved for the update flag

    lut[i] = b5 | g6 | r5;
  }
}

// Set the hardware drawing window
void GC9203_Driver::setWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
  // Re-apply rotation setting, as it might be changed by other operations
  writeCommand(0x0003); // Entry Mode
  switch (_rotation) {
    case 0: writeData(0x1000); break;
    case 1: writeData(0x1030); break;
    case 2: writeData(0x1028); break;
    case 3: writeData(0x1018); break;
  }

  // Apply window coordinates based on rotation
  if (_rotation == 0 || _rotation == 2) { // Portrait
    writeCommand(0x0037); writeData(x1 + 24);
    writeCommand(0x0036); writeData(x2 + 24);
    writeCommand(0x0039); writeData(y1);
    writeCommand(0x0038); writeData(y2);
  } else { // Landscape
    writeCommand(0x0037); writeData(y1 + 24);
    writeCommand(0x0036); writeData(y2 + 24);
    writeCommand(0x0039); writeData(x1);
    writeCommand(0x0038); writeData(x2);
  }
  writeCommand(0x0022); // Prepare to write to GRAM
}

// Mark a pixel and its row as "dirty" (needs update)
void GC9203_Driver::updateFlag(int16_t x, int16_t y) {
  // Set the lowest bit (update flag)
  framebuffer[y][x] |= 0x01;
  // Mark the corresponding row in the quick-skip array
  update_row[y / 8] |= (1 << (y % 8));
}

// --- Public API Functions ---

// Push the "dirty" parts of the framebuffer to the screen
// This is your partial refresh algorithm
void GC9203_Driver::display() {
  for (uint16_t row = 0; row < GC9203_HEIGHT; row++) {
    // Check the quick-skip array. If row isn't dirty, skip it.
    if ((update_row[row / 8] & (1 << (row % 8))) == 0) continue;

    int16_t x_start = -1;
    int16_t prev_x = -1;

    for (uint16_t x = 0; x < GC9203_WIDTH; x++) {
      if (framebuffer[row][x] & 0x01) { // Check if update flag (bit 0) is set
        if (x_start == -1) {
          x_start = x; // Start of a new dirty block
        } else if (prev_x != -1 && (x - prev_x) > SEPARATION_THRESHOLD) {
          // Block is broken, send the previous block
          setWindow(x_start, row, prev_x, row);
          for (int i = x_start; i <= prev_x; i++) {
            framebuffer[row][i] &= ~0x01; // Clear update flag
            uint16_t color16 = lut[framebuffer[row][i] & 0xFE]; // Get color from bits 7-1
            writeData(color16);
          }
          x_start = x; // Start a new block
        }
        prev_x = x;
      }
    }

    // Send the final block in the row
    if (x_start != -1) {
      setWindow(x_start, row, prev_x, row);
      for (int i = x_start; i <= prev_x; i++) {
        framebuffer[row][i] &= ~0x01; // Clear update flag
        uint16_t color16 = lut[framebuffer[row][i] & 0xFE];
        writeData(color16);
      }
    }

    // Clear the row's quick-skip flag
    update_row[row / 8] &= ~(1 << (row % 8));
  }
}

// Clear the internal framebuffer (doesn't update screen)
void GC9203_Driver::clearDisplay() {
  // Set the entire framebuffer to 0 and mark all rows for update
  memset(framebuffer, 0x00, sizeof(framebuffer));
  memset(update_row, 0xFF, sizeof(update_row)); // Mark all rows as dirty
}

// Fill the internal framebuffer with a color
void GC9203_Driver::fillScreen(uint8_t color) {
  uint8_t color_8bit = (color << 1) & 0xFE; // Use bits 7-1 for color
  for (int y = 0; y < GC9203_HEIGHT; y++) {
    for (int x = 0; x < GC9203_WIDTH; x++) {
      framebuffer[y][x] = color_8bit | 0x01; // Set color and update flag
    }
  }
  // Mark all rows as dirty
  memset(update_row, 0xFF, sizeof(update_row));
}

// Draw a single pixel to the internal framebuffer
void GC9203_Driver::drawPixel(int16_t x, int16_t y, uint8_t color) {
  // Bounds check
  if (x < 0 || x >= GC9203_WIDTH || y < 0 || y >= GC9203_HEIGHT) {
    return;
  }
  
  // Set color (bits 7-1) and update flag (bit 0)
  framebuffer[y][x] = ((color << 1) & 0xFE) | 0x01;
  
  // Mark row as dirty for partial refresh
  updateFlag(x, y);
}

// --- New Graphics Primitives ---

// Fill a rectangle
void GC9203_Driver::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) {
  // Simple implementation: call drawPixel repeatedly
  // A more optimized version would write to the framebuffer array directly
  for (int16_t i = x; i < x + w; i++) {
    for (int16_t j = y; j < y + h; j++) {
      drawPixel(i, j, color);
    }
  }
}

// Draw a rectangle (outline)
void GC9203_Driver::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) {
  drawLine(x, y, x + w - 1, y, color); // Top
  drawLine(x, y, x, y + h - 1, color); // Left
  drawLine(x + w - 1, y, x + w - 1, y + h - 1, color); // Right
  drawLine(x, y + h - 1, x + w - 1, y + h - 1, color); // Bottom
}

// Draw a line using Bresenham's algorithm
void GC9203_Driver::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color) {
  int16_t dx = abs(x1 - x0);
  int16_t dy = -abs(y1 - y0);
  int16_t sx = (x0 < x1) ? 1 : -1;
  int16_t sy = (y0 < y1) ? 1 : -1;
  int16_t err = dx + dy;
  
  while (true) {
    drawPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    int16_t e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}