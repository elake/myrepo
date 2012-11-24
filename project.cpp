/*
  Table of Contents
  

  Sec0.1: Included Files and libraries
  Sec0.2: Global Variables and Constants
  Sec0.3: Non-Constant Globals and Cache Data
  Sec0.4: Functions
  Sec?: Arduino Setup Procedure
  Sec?: Arduino Loop Procedure
 */

//*****************************************************************************
//                     Sec0.1: Included Files and Libraries
//*****************************************************************************

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <SD.h>
#include <stdlib.h>
#include "mem_syms.h"
#include "tile.h"
#include "checker.h"
#include "lcd_image.h"

//*****************************************************************************
//                    Sec0.2: Global Variables and Constants
//*****************************************************************************

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 160
#define BORDER_WIDTH 2 // Border surrounding the playable area

const uint8_t CHECKERS_PER_SIDE = 12; // 12 pieces per side, as per game rules

const uint8_t NUM_TILES = 64; // standard 8x8 board

#define TILE_SIZE (SCREEN_WIDTH - (2 *  BORDER_WIDTH) / 8 // tile width and height in pixels

#define SD_CS 5
#define TFT_CS 6
#define TFT_DC 7
#define TFT_RST 8

//*****************************************************************************
//                   Sec0.3: Non-Constant Globals and Cache Data     
//*****************************************************************************

Checker red_checkers[CHECKERS_PER_SIDE];  // the array of red checkers
Checker black_checkers[CHECKERS_PER_SIDE]; // the array of black checkers

Tile tile_array[NUM_TILES];

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

lcd_image_t cb_img = {"cb.lcd", SCREEN_WIDTH, SCREEN_HEIGHT};



//*****************************************************************************
//                             Sec0.4: Functions     
//*****************************************************************************


uint8_t coord_to_tile(uint8_t x, uint8_t y)
{
  /*
    Map (x,y) coordinate to corresponding tile in 64-tile array, where:

    x: the horizontal tile
    y: the vertical tile

   */
  return ((8*y) + x); // 8 - the number of tiles per row
}

uint8_t* tile_to_coord(uint8_t tile_num)
{
  /* 
     maps a tile number to x,y coordinate, where:
     
     tile_num - the tile index of the tile array
  */  
  uint8_t x_y[2];
  x_y[0] = tile_num % 8;
  x_y[1] = tile_num / 8;
  return x_y;

}

void draw_tile(uint8_t tile_index)
{
  /*
    draw the tile tile_array[tile_index], and the checker it
    contains (if any), to the lcd display; we can assume the image size
    and screen size are equal, since there's no real reason to scroll in
    a checkers game, we can use the same vars for icol, scol and irow, srow,
    where:

    tile_index: the index of the tile in *tile_array to draw

    uses globals: cb_img, tft, TILE_SIZE
        
   */
  uint8_t* x_y = tile_to_coord(tile_index);
  uint16_t col = x_y[0] * TILE_SIZE;
  uint16_t row = x_y[1] * TILE_SIZE;

  // draw over this tile:
  lcd_image_draw(&cb_img, &tft, col, row, col, row, TILE_SIZE, TILE_SIZE);
  
  // and draw the checker it contains, if any:

}


//*****************************************************************************
//                          Sec?: Setup Procedure     
//*****************************************************************************

void setup()
{
  // Fill the red checker array
  for (uint8_t i = 0; i < CHECKERS_PER_SIDE; i++) {	
    uint8_t x_ti = (2*(i%4) + (((i/4) + 1)%2)); // fancy maths
    uint8_t y_ti = i/4;
    red_checkers[i].x_tile = x_ti;
    red_checkers[i].y_tile = y_ti;

    // record that the proper tiles contain these checkers
    tile_array[coord_to_tile(x_ti, y_ti)].has_checker = 'r';
    tile_array[coord_to_tile(x_ti, y_ti)].checker_num = i;
  }

  // Fill the black checker array
  for (uint8_t i = 0; i < CHECKERS_PER_SIDE; i++) {
    // x_ti, y_ti first declared above
    uint8_t x_ti = (2*(i%4) + ((i/4)%2));
    uint8_t y_ti = (i/4) + 5;
    black_checkers[i].x_tile = x_ti;
    black_checkers[i].y_tile = y_ti;

    // record that the proper tiles contain these checkers
    tile_array[coord_to_tile(x_ti, y_ti)].has_checker = 'b';
    tile_array[coord_to_tile(x_ti, y_ti)].checker_num = i;
  }

  draw_tile(1);
}

//*****************************************************************************
//                        Sec?: Arduino Loop Procedure
//*****************************************************************************

void loop()
{

}
