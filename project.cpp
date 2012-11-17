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
#include <mem_syms.h>
#include "tile.h"
#include "checker.h"
#include "lcd_image.cpp"


//*****************************************************************************
//                    Sec0.2: Global Variables and Constants
//*****************************************************************************


//*****************************************************************************
//                   Sec0.3: Non-Constant Globals and Cache Data     
//*****************************************************************************

Checker red_checkers[12];  // the array of red checkers
Checker black_checkers[12]; // the array of black checkers



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
  return ((8*y) + x);
}
//*****************************************************************************
//                          Sec?: Setup Procedure     
//*****************************************************************************

void setup{
  // Fill the red checker array
  for (uint8_t i = 0; i < 12; i++) { 
    red_checker[i].x_tile = (2*(i%4) + (((i/4) + 1)%2));
    red_checker[i].y_tile = i/4;

  }

  // Fill the black checker array
  for (uint8_t i = 0; i < 12; i++) {
    black_checker[i].x_tile = (2*(i%4) + ((i/4)%2));
    black_checker[i].y_tile = (i/4) + 5;

  }

}

//*****************************************************************************
//                        Sec?: Arduino Loop Procedure
//*****************************************************************************

void loop{}
