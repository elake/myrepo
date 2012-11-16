/* Simple Image Drawing
 *
 * Draws an image to the screen.  The image is stored in "parrot.lcd" on 
 * the SD card.  The image file contains only raw pixel byte-pairs.
 */

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library
#include <SPI.h>
#include <SD.h>

#include "lcd_image.h"

// standard U of A library settings, assuming Atmel Mega SPI pins
#define SD_CS    5  // Chip select line for SD card
#define TFT_CS   6  // Chip select line for TFT display
#define TFT_DC   7  // Data/command line for TFT
#define TFT_RST  8  // Reset line for TFT (or connect to +5V)

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

lcd_image_t map_image = { "parrot.lcd", 128, 128 };

void setup(void) {
  Serial.begin(9600);

  // If your TFT's plastic wrap has a Red Tab, use the following:
  tft.initR(INITR_REDTAB);   // initialize a ST7735R chip, red tab
  // If your TFT's plastic wrap has a Green Tab, use the following:
  //tft.initR(INITR_GREENTAB); // initialize a ST7735R chip, green tab

  Serial.print("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("failed!");
    return;
  }
  Serial.println("OK!");

  // clear to yellow
  tft.fillScreen(tft.Color565(0xff, 0xff, 0x00));

  lcd_image_draw(&map_image, &tft, 0, 0, 0, 0, 128, 128);
  lcd_image_draw(&map_image, &tft, 0, 0, 64, 64, 64, 64);
}

void loop() {
}
