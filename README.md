# Low-Level LCD Driver for GC9203

[Insert a GIF later]

## 1. Project Description

This is a low-level C++ driver for the GC9203 LCD controller, written entirely from scratch. It is designed to be lightweight and efficient for memory-constrained microcontrollers like the ESP8266. The entire library was developed in the Arduino IDE by meticulously interpreting the official device datasheet and SPI timing diagrams.

## 2. Tech Stack

* **Language:** C++
* **Hardware:** ESP8266, GC9203 LCD Controller
* **Protocol:** SPI (Serial Peripheral Interface)
* **Development:** Arduino IDE
* **Debugging Tools:** Logic Analyzer

## 3. Core Features

* **From-Scratch Implementation:** No third-party graphics libraries were used. All SPI communication and drawing logic are custom-built.
* **Optimized Memory Usage:** Implemented a partial refresh algorithm instead of a full framebuffer. This approach reduced RAM usage by over 60%, making it ideal for devices with limited memory.
* **Graphics Primitives:** A C++ library of essential drawing functions, including `drawLine`, `drawRectangle`, `drawCharacter`, and `fillScreen`.
* **Gamma Correction:** Integrated a Gamma correction curve to ensure accurate color reproduction and proper contrast, preventing washed-out images.

## 4. Challenges & Solutions

This project involved significant hardware-level debugging. My primary challenges were not in software logic, but in making the hardware respond correctly.

### Challenge 1: Hardware Initialization Failure
* **Symptom:** The screen remained blank. Despite sending what I believed were the correct initialization commands from the datasheet, the LCD controller failed to initialize.
* **Solution:** I used a logic analyzer to physically probe and debug the SPI signals (MOSI, MISO, CLK, CS). By capturing the traffic, I discovered that my register-level commands had incorrect timing and formatting. I was able to cross-reference the logic analyzer's output with the datasheet's timing diagrams to correct the initialization sequence and successfully boot the display.

### Challenge 2: Optimizing Refresh Rate (FPS)
* **Symptom:** The FPS was low when drawing complex shapes, but simply increasing the SPI clock speed (MHz) led to signal instability.
* **Solution:** I optimized the driver at the register-command level. Instead of sending single-pixel commands, I re-architected the drawing functions to send data in large, continuous bursts. This dramatically reduced the SPI protocol overhead (the time spent sending commands vs. pixel data), resulting in a significant FPS improvement even at the same stable SPI clock speed.

### Challenge 3: Poor Color Reproduction
* **Symptom:** The default 16-bit colors appeared washed out, and the contrast was poor.
* **Solution:** I researched the non-linear brightness response of LCDs and implemented a Gamma correction lookup table. This table maps linear input color values to the display's native curve, resulting in vibrant, accurate colors and deep contrast.

## 5. How to Build and Use

Place GC9203_Driver.h and GC9203_Driver.cpp in your Arduino project folder.

Include the header: #include "GC9203_Driver.h"

Create a driver instance with your pin definitions:

#define CS_PIN 15
#define DC_PIN 4
#define RST_PIN 5

// SCK and MOSI use the default hardware SPI pins
GC9203_Driver lcd = GC9203_Driver(SCK_PIN, MOSI_PIN, CS_PIN, DC_PIN, RST_PIN);


Initialize in setup():

void setup() {
  lcd.begin();
}


Draw to the framebuffer and call display() to update the screen:

void loop() {
  lcd.fillRect(20, 20, 50, 50, COLOR_RED);
  lcd.drawLine(0, 0, 127, 219, COLOR_GREEN);

  // Push all changes to the screen
  lcd.display();
}

