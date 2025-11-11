#include "GC9203_Driver.h"

// Define your pins. These match your original .ino
// On ESP8266, default HSPI is:
// SCK  = GPIO 14
// MOSI = GPIO 13
// MISO = GPIO 12 (unused)
#define SCK_PIN 14
#define MOSI_PIN 13
#define CS_PIN 15
#define DC_PIN 4
#define RST_PIN 5

// Create an instance of the driver
GC9203_Driver lcd = GC9203_Driver(SCK_PIN, MOSI_PIN, CS_PIN, DC_PIN, RST_PIN);

// Define some 8-bit colors (using the 2:3:2 R:G:B scheme)
// R (bits 7-6), G (bits 5-3), B (bits 2-1)
// Bit 0 is the update flag, so we define colors from 0-127
#define COLOR_BLACK   0
#define COLOR_RED     (0b11 << 5) // 96
#define COLOR_GREEN   (0b111 << 2) // 28
#define COLOR_BLUE    (0b11) // 3
#define COLOR_WHITE   (0b1111111) // 127
#define COLOR_YELLOW  (COLOR_RED | COLOR_GREEN)
#define COLOR_CYAN    (COLOR_GREEN | COLOR_BLUE)
#define COLOR_MAGENTA (COLOR_RED | COLOR_BLUE)


void setup() {
  Serial.begin(115200); // Use a faster baud rate
  Serial.println("GC9203 Driver Example");

  lcd.begin();
  
  // Test fillScreen
  lcd.fillScreen(COLOR_BLACK);
  lcd.display(); // Push the fill to the screen
  delay(1000);

  // Test graphics primitives
  lcd.fillRect(20, 20, 50, 50, COLOR_RED);
  lcd.drawRect(80, 20, 40, 30, COLOR_BLUE);
  
  lcd.display(); // Push the rectangles
  delay(1000);

  lcd.drawLine(0, 0, GC9203_WIDTH - 1, GC9203_HEIGHT - 1, COLOR_GREEN);
  lcd.drawLine(GC9203_WIDTH - 1, 0, 0, GC9203_HEIGHT - 1, COLOR_YELLOW);

  lcd.display(); // Push the lines
  delay(1000);
}

void loop() {
  // Test pixel-by-pixel update
  for (int i=0; i < 20; i++) {
    int x = random(GC9203_WIDTH);
    int y = random(GC9203_HEIGHT);
    lcd.drawPixel(x, y, COLOR_WHITE);
  }
  
  // Only the 20 new pixels (and their neighbors, due to the
  // SEPARATION_THRESHOLD) will be sent to the display.
  // This is much faster than a full screen refresh.
  unsigned long start = micros();
  lcd.display();
  unsigned long duration = micros() - start;
  
  Serial.printf("Partial refresh of 20 pixels took %lu microseconds\n", duration);
  
  delay(500); // Wait half a second
}