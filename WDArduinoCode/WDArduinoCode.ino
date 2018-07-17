#include <GxEPD.h>
#include <GxGDE0213B1/GxGDE0213B1.cpp>
#include <GxIO/GxIO_SPI/GxIO_SPI.cpp>
#include <GxIO/GxIO.cpp>

// Initialize SPI
GxIO_Class io(SPI, SS, D3, D4);
// Adafruit GFX compatible class
GxEPD_Class display(io);

void setup() {
  // Initialize display
  display.init();
  // Set black text
  display.setTextColor(GxEPD_BLACK);
  // Text location: x = 30, y = 60
  display.setCursor(30,60);
  display.print("Sun shines");
  // Update commands to screen
  display.update();

}

void loop() {
}
