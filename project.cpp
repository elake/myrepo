/*
  Table of Contents
  

  Sec0.0: Included Files and libraries
  Sec0.1: Global Variables and Constants
    Sub0.10: lcd screen wiring
    Sub0.11: screen constants
    Sub0.12: checkers settings
    Sub0.13: joystick range information
    Sub0.14: joystick pins
  Sec0.2: Non-Constant Globals and Cache Data
    Sub0.20: checker arrays
    Sub0.21: tile array
    Sub0.22: tft object
    Sub0.23: card object
    Sub0.24: lcd image objects
    Sub0.25: joystick readings & remap values
  Sec0.3: Functions
    Sub0.30: tile -> coordinate // coordinate -> tile maps
    Sub0.31: tile drawing procedure
    Sub0.32: tile highlighting
    Sub0.33: graveyard drawing
    Sub0.34: turn indicator drawing
    Sub0.35: jump and force jump computing
  Sec0.4: Arduino Setup Procedure
    Sub0.40: serial monitor & sd card preliminaries
    Sub0.41: drawing the checker board
    Sub0.42: filling the checker and tile arrays
    Sub0.43: set up pins
    Sub0.44: calibrate the joystick
  Sec0.5: Arduino Loop Procedure
 */

//*****************************************************************************
//                     Sec0.0: Included Files and Libraries
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
//                    Sec0.1: Global Variables and Constants
//****************************************************************************
// Sub0.10: lcd screen wiring
#define SD_CS 5
#define TFT_CS 6
#define TFT_DC 7
#define TFT_RST 8

// Sub0.11: screen constants
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 160
#define BORDER_WIDTH 4 // Border surrounding the playable area
#define CBSQUARE_SIZE 8 // standard 8x8 checker board

#define GRAVSTART_BLUEX 18 // the x, y start positions of the checkers in 
#define GRAVSTART_REDX 66  // the graveyard
#define GRAVSTART_Y 128 // both start at the same horizontal level
#define GRAV_ROWSEP 1 // the vertical separation between each row in graveyard

// Sub0.12: checkers settings
const uint8_t CHECKERS_PER_SIDE = 12; // 12 pieces per side, as per game rules
const uint8_t NUM_TILES = 64; // standard 8x8 board
#define TILE_SIZE ((128-8)/8)// tile width and height in pixels
#define GRAV_PIECEWIDTH 11 // the width/height of a graveyard checker piece
#define GRAV_PIECEHEIGHT 10

// Sub0.13: joystick range information
#define VOLT_MIN 0          // 0 volts 
#define VOLT_MAX 1023       // 5 volts
#define JOY_REMAP_MAX 1000  // For remapping 0-1023 to ~-1000 - 1000

// Sub0.14: joystick pins
#define JOYSTICK_HORIZ 0    // Analog input A0 - horizontal
#define JOYSTICK_VERT  1    // Analog input A1 - vertical
#define JOYSTICK_BUTTON 9   // Digitial input pin 9 for the button

// Sub0.15: mode mapping
#define TURN_RED 1
#define TURN_BLUE -1
#define SETUP_MODE 0
#define PLAY_MODE 1
#define GAMEOVER_MODE 2
#define HIGHLIGHT_MOVES 3
#define HIGHLIGHT_JUMPS 4

//*****************************************************************************
//                   Sec0.2: Non-Constant Globals and Cache Data     
//*****************************************************************************

// Sub0.20: checker arrays
Checker red_checkers[CHECKERS_PER_SIDE];  // the array of red checkers
Checker blue_checkers[CHECKERS_PER_SIDE]; // the array of blue checkers
Checker* player_checkers;

// Sub0.21: tile array
Tile tile_array[NUM_TILES];

// Sub0.22: tft object
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Sub0.23: card object
Sd2Card card;

// Sub0.24: lcd image objects
// empty checkerboard image
lcd_image_t cb_img = {"c.lcd", SCREEN_WIDTH, SCREEN_HEIGHT};
// checkerboard image with red pieces
lcd_image_t cbr_image = {"cr.lcd", SCREEN_WIDTH, SCREEN_HEIGHT};
// checkerboard image with blue pieces
lcd_image_t cbb_image = {"cb.lcd", SCREEN_WIDTH, SCREEN_HEIGHT};
// checkerboard image with red kinged pieces
lcd_image_t cbrk_image = {"crk.lcd", SCREEN_WIDTH, SCREEN_HEIGHT};
// checkerboard image with blue kinged pieces
lcd_image_t cbbk_image = {"cbk.lcd", SCREEN_WIDTH, SCREEN_HEIGHT};
// checkerboard image with fully populated graveyard
lcd_image_t cbg_image = {"g.lcd", SCREEN_WIDTH, SCREEN_HEIGHT};

// Sub0.25: joystick readings & remap values
int joy_x;           // x and 
int joy_y;           // y positions of the joystick
int joy_min_x;       // remap minima for the x and
int joy_min_y;       // y readings from the joystick

// Sub?: tile highlighting
uint8_t tile_highlighted = 36;
uint8_t subtile_highlighted;
uint8_t tile_selected = 0; // whether we have selected a tile
uint8_t subtile_selected = 0;
uint16_t delay_time = 100;
Checker active_checker;
bool checker_owned;
bool checker_must_jump;

// Sub?: game state
uint8_t game_state = SETUP_MODE; // we need to set up the board
uint8_t red_dead = 0;
uint8_t blue_dead = 0;
int8_t player_turn = TURN_BLUE;
uint8_t recompute_force_jumps = 1;
uint8_t current_state_a = 0; // debounce
uint8_t no_fjumps = 1;
uint8_t move_ok = 0;
uint8_t bouncer = 1;

//*****************************************************************************
//                             Sec0.3: Functions     
//*****************************************************************************

// Sub0.30: tile -> coordinate // coordinate -> tile maps
uint8_t coord_to_tile(uint8_t x, uint8_t y)
{
  /*
    Map (x,y) coordinate to corresponding tile in 64-tile array, where:

    x: the horizontal tile
    y: the vertical tile

   */
  if (x < 0 || x > 7 || y < 0 || y > 7){
    return 0;
  }
  return ((8*y) + x);
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

// Sub0.31: tile drawing procedure
void draw_tile(Tile* tile_array, Checker* red_checkers, Checker* blue_checkers,
	       uint8_t tile_index)
{
  /*
    draw the tile tile_array[tile_index], and the checker it
    contains (if any), to the lcd display; we can assume the image size
    and screen size are equal, since there's no real reason to scroll in
    a checkers game, we can use the same vars for icol, scol and irow, srow,
    where:

    tile_array: the address of the array of tiles.
    tile_index: the index of the tile in *tile_array to draw
    red_checkers: the address of the red checker array
    blue_checkers: the address of the blue checker array
    

    uses globals: cb_img, tft, TILE_SIZE
        
   */
  uint8_t* x_y = tile_to_coord(tile_index);
  uint16_t col = (x_y[0] * TILE_SIZE) + BORDER_WIDTH;
  uint16_t row = (x_y[1] * TILE_SIZE) + BORDER_WIDTH;

  // draw over this tile with a symmetric tile depending on what 
  // checker it contains, and, if it contains one, whether or not it's kinged:
  if (tile_array[tile_index].has_checker == 0){
    // draw the blank tile
    lcd_image_draw(&cb_img, &tft, col, row, col, row, TILE_SIZE, TILE_SIZE);
  }
  else if (tile_array[tile_index].has_checker == TURN_RED){
    // draw the tile with a red checker on it
    if (red_checkers[tile_array[tile_index].checker_num].is_kinged) {
      lcd_image_draw(&cbrk_image, &tft, col, row, col, row, 
		     TILE_SIZE, TILE_SIZE);
    }
    else {
      lcd_image_draw(&cbr_image, &tft, col, row, col, row, TILE_SIZE, 
		     TILE_SIZE);
    }
  }
  else {
    // draw the tile with a blue checker on it
    if (blue_checkers[tile_array[tile_index].checker_num].is_kinged) {
      lcd_image_draw(&cbbk_image, &tft, col, row, col, row, 
		     TILE_SIZE, TILE_SIZE);
    }
    else {
      lcd_image_draw(&cbb_image, &tft, col, row, col, row, TILE_SIZE, 
		     TILE_SIZE);
    }
  }  
}

// Sub0.32: tile highlighting
void highlight_tile(uint8_t tile_num, int8_t mode, uint8_t sel)
{
  /*
    draw a rect around the tile given by the x, y coordinates, with color 
    dependent on whose turn it is and whether the tile has been highlighted,
    where:
    x, y: the x, y tile to highlight
    mode: what mode to highlight the tile in
    sel: whether the tile has been selected

    Uses consts: BORDER_WIDTH, TILE_SIZE
    Uses globals: tft
   */
  int color;
  if (sel){
    color = ST7735_WHITE;
  }
  else if (mode == TURN_RED){
    color = ST7735_RED;
  }
  else if (mode == TURN_BLUE){
    color = ST7735_BLUE;
  }
  else if (mode == HIGHLIGHT_MOVES){
    color = ST7735_GREEN;
  }
  else if (mode == HIGHLIGHT_JUMPS){
    color = ST7735_MAGENTA;
  }
  uint8_t* x_y = tile_to_coord(tile_num);
  uint8_t col = BORDER_WIDTH + (TILE_SIZE * x_y[0]);
  uint8_t row = BORDER_WIDTH + (TILE_SIZE * x_y[1]);
  
  tft.drawRect(col, row, TILE_SIZE, TILE_SIZE, color);
}

// Sub0.33: graveyard drawing
void populate_graveyard(uint8_t num_dead, int8_t turn)
{

  /*
    this function is called when a piece from either player dies, and puts the
    corresponding death number into the graveyard at the bottom of the screen,
    where:

    num_dead: the number of checkers dead for either side
    turn: whose turn it was when the checker died, so that a checker from the
          opposite side is committed to the graveyard
    
    uses globals: cbg_image, tft

  */
  // invariant: 1 <= num_dead <= 12
  if (( num_dead < 1 || num_dead > CHECKERS_PER_SIDE)){
    Serial.println("You've called the populate graveyard function improperly");
    while(1) {} // loop until plug is pulled.
  }

  uint8_t dead_index = num_dead - 1; // for indexing at 0
  uint8_t col_index;
  uint8_t row_index = dead_index % 3;
  uint8_t x;
  uint8_t y;

  if (turn == TURN_RED) { // someone's killed a blue piece!
    col_index = dead_index / 3;
    x = GRAVSTART_BLUEX + (col_index * GRAV_PIECEWIDTH);
    y = GRAVSTART_Y + (row_index * GRAV_PIECEHEIGHT) + row_index;
    lcd_image_draw(&cbg_image, &tft, x, y, x, y, 
		   GRAV_PIECEWIDTH, GRAV_PIECEHEIGHT);
  }
  else if (turn == TURN_BLUE) { // someone's killed a red piece!
    col_index = 3 - dead_index / 3;
    x = GRAVSTART_REDX + (col_index * GRAV_PIECEWIDTH);
    y = GRAVSTART_Y + (row_index * GRAV_PIECEHEIGHT) + row_index;
    lcd_image_draw(&cbg_image, &tft, x, y, x, y, 
		   GRAV_PIECEWIDTH, GRAV_PIECEHEIGHT);
  }
}

// Sub0.34: turn indicator drawing
void turn_indicator(int8_t turn)
{
  /*
    this function is called to indicate on the lcd display whose turn it is,
    and does so by coloring the border their respective color, where:

    turn: whose turn we would like to indicate on screen

    uses globals: cbr_image, cbb_image, tft
   */

  if (turn == -1){
    lcd_image_draw(&cbr_image, &tft, 0, 0, 0, 0, SCREEN_WIDTH, BORDER_WIDTH);
    lcd_image_draw(&cbr_image, &tft, 0, 0, 0, 0, BORDER_WIDTH, SCREEN_WIDTH);
    lcd_image_draw(&cbr_image, &tft, SCREEN_WIDTH - BORDER_WIDTH, 0, 
		   SCREEN_WIDTH - BORDER_WIDTH, 0, BORDER_WIDTH, SCREEN_WIDTH);
    lcd_image_draw(&cbr_image, &tft, 0, SCREEN_WIDTH - BORDER_WIDTH, 0, 
		   SCREEN_WIDTH - BORDER_WIDTH, SCREEN_WIDTH, BORDER_WIDTH);
  }
  else {
    lcd_image_draw(&cbb_image, &tft, 0, 0, 0, 0, SCREEN_WIDTH, BORDER_WIDTH);
    lcd_image_draw(&cbb_image, &tft, 0, 0, 0, 0, BORDER_WIDTH, SCREEN_WIDTH);
    lcd_image_draw(&cbb_image, &tft, SCREEN_WIDTH - BORDER_WIDTH, 0, 
		   SCREEN_WIDTH - BORDER_WIDTH, 0, BORDER_WIDTH, SCREEN_WIDTH);
    lcd_image_draw(&cbb_image, &tft, 0, SCREEN_WIDTH - BORDER_WIDTH, 0, 
		   SCREEN_WIDTH - BORDER_WIDTH, SCREEN_WIDTH, BORDER_WIDTH);
  }
}

//Sub0.35: jump and force jump computing
uint8_t* compute_checker_jumps(Tile* tile_array, Checker* checker,
		     int8_t opp_color)
{
  /*
    this function checks the potential jumps of a given checker by checking
    the two respective diagonal tiles it may have an opponent in (all four
    if the checker is kinged) as well as the next diagonal space over
   
   */
  int8_t dir = opp_color;
  if (tile_array[coord_to_tile(checker->x_tile -1, 
			    checker->y_tile - dir)].has_checker == opp_color) {
    // Serial.print("Checker's x_tile: ");
    // Serial.println(checker->x_tile);
    // Serial.print("Checker's y_tile: ");
    // Serial.println(checker->y_tile);
    // Serial.print("Top left has checker: ");
    // Serial.println(tile_array[coord_to_tile(checker->x_tile -1, 
    // 					  checker->y_tile - dir)].has_checker);
    // delay(10000);
    if (tile_array[coord_to_tile(checker->x_tile - 2,
				  checker->y_tile - 2*dir)].has_checker == 0) {
      // Serial.println("Yes");
      checker->jumps[0] = coord_to_tile(checker->x_tile - 2, 
				     checker->y_tile - 2*dir);
      checker->must_jump = 1;
    }
  }
  if (tile_array[coord_to_tile(checker->x_tile + 1 , 
			    checker->y_tile - dir)].has_checker == opp_color) {
    if (tile_array[coord_to_tile(checker->x_tile + 2,
				 checker->y_tile - 2*dir)].has_checker == 0) {
      checker->jumps[1] = coord_to_tile(checker->x_tile + 2, 
				     checker->y_tile - 2*dir);
      checker->must_jump = 1;
    }
  }
  if(checker->is_kinged){
    if (tile_array[coord_to_tile(checker->x_tile - 1, 
			    checker->y_tile + dir)].has_checker == opp_color) {
      if (tile_array[coord_to_tile(checker->x_tile - 2,
				  checker->y_tile + 2*dir)].has_checker == 0) {
	checker->jumps[2] = coord_to_tile(checker->x_tile - 2, 
				       checker->y_tile + 2*dir);
	checker->must_jump = 1;
      }
    }
    if (tile_array[coord_to_tile(checker->x_tile + 1, 
			    checker->y_tile + dir)].has_checker == opp_color) {
      if (tile_array[coord_to_tile(checker->x_tile + 2,
				  checker->y_tile + 2*dir)].has_checker == 0) {
	checker->jumps[3] = coord_to_tile(checker->x_tile + 2, 
				       checker->y_tile + 2*dir);
	checker->must_jump = 1;
      }
    }
  }
}

void compute_checker_moves(Tile* tile_array, Checker* checker, 
			   int8_t opp_color)
{
  int8_t dir = opp_color;
  if (tile_array[coord_to_tile(checker->x_tile -1, 
			    checker->y_tile - dir)].has_checker == 0) {
    checker->moves[0] = coord_to_tile(checker->x_tile -1, checker->y_tile - 
				      dir);
  }
  if (tile_array[coord_to_tile(checker->x_tile + 1 , 
			   checker->y_tile - dir)].has_checker == 0) {
    checker->moves[1] = coord_to_tile(checker->x_tile + 1, checker->y_tile - 
				      dir);
  }
  if(checker->is_kinged){
    if (tile_array[coord_to_tile(checker->x_tile - 1, 
			    checker->y_tile + dir)].has_checker == 0) {
      checker->moves[2] = coord_to_tile(checker->x_tile - 1, checker->y_tile +
					dir);
    }
    if (tile_array[coord_to_tile(checker->x_tile + 1, 
			   checker->y_tile + dir)].has_checker == 0) {
      checker->moves[3] = coord_to_tile(checker->x_tile + 1, checker->y_tile +
					dir);
    }
  }
}

uint8_t compute_moves(Tile* tile_array, Checker* checkers, 
			    int8_t opp_color)
{
  uint8_t no_fjumps = 1;
  for (uint8_t i = 0; i < CHECKERS_PER_SIDE; i++){
    compute_checker_moves(tile_array, &checkers[i], opp_color);
    compute_checker_jumps(tile_array, &checkers[i], opp_color);
    if (checkers[i].must_jump){
      no_fjumps = 0;
    }
  }
  return no_fjumps;
}

void highlight_moves(Tile* tile_array, uint8_t tile_num, 
		     Checker* player_checkers)
{
  uint8_t tile_hilight;
  for (uint8_t i = 0; i < 4; i++){
    tile_hilight = player_checkers[tile_array[tile_num].checker_num].moves[i];
    if (tile_hilight){
      highlight_tile(tile_hilight, HIGHLIGHT_MOVES, 0);
    }
  }
}
void highlight_jumps(Tile* tile_array, uint8_t tile_num, 
		     Checker* player_checkers)
{
  uint8_t tile_hilight;
  for (uint8_t i = 0; i < 4; i++){
    tile_hilight = player_checkers[tile_array[tile_num].checker_num].jumps[i];
    // Serial.print("jump "); Serial.print(i); Serial.print(": ");
    // Serial.println(tile_hilight);
    if (tile_hilight){
      highlight_tile(tile_hilight, HIGHLIGHT_JUMPS, 0);
    }
  }
}

uint8_t selection_matches_move(uint8_t selection, Checker* active_checker,
			       uint8_t check_moves)
{
  uint8_t matches = 0;

  for (uint8_t i = 0; i < 4; i++){
    if (check_moves) {
      if (selection == active_checker->moves[i]){
	matches = 1;
      }
    }
    else {
      if (selection == active_checker->jumps[i]){
	matches = 1;
      }
    }
  }
  Serial.println(matches);
  return matches;
}

uint8_t check_can_move(Checker* active_checker)
{
  uint8_t matches = 0;
  for (uint8_t i = 0; i < 4; i++){
    if (active_checker->moves[i]){
      matches = 1;
    }
  }
  return matches;
}			    

void move_piece(Tile* tile_array, Checker* active_checker, 
		uint8_t active_tile, uint8_t destination_tile)
{
  tile_array[destination_tile].has_checker = 
    tile_array[active_tile].has_checker;

  tile_array[active_tile].has_checker = 0;
  
  uint8_t* x_y = tile_to_coord(destination_tile);
  active_checker->x_tile = x_y[0];
  active_checker->y_tile = x_y[1];
}

//*****************************************************************************
//                          Sec?: Setup Procedure     
//*****************************************************************************

void setup()
{
  // Sub0.40: serial monitor & sd card preliminaries
  Serial.begin(9600);
  tft.initR(INITR_REDTAB);   // initialize a ST7735R chip, red tab

  Serial.print("Avail mem (bytes):");
  Serial.println(AVAIL_MEM);

  Serial.print("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("failed!");
    return;
  }
  Serial.println(" succeeded!");

  if (!card.init(SPI_HALF_SPEED, SD_CS)) {
    Serial.println("Raw SD Initialization has failed");
    while (1) {};  // Just wait, stuff exploded.
  }

  // Sub0.41 drawing the checker board
  tft.fillScreen(0); // fill to black  
  lcd_image_draw(&cb_img, &tft, 0, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

  // Sub0.42: filling the checker and tile arrays

  // Sub0.43: set up pins
  // joystick
  pinMode(JOYSTICK_BUTTON, INPUT);     // tell arduino to read joybutton inputs
  digitalWrite(JOYSTICK_BUTTON, HIGH); // button presses set voltage to LOW!!


  // Sub0.44: calibrate the joystick
  joy_x = analogRead(JOYSTICK_HORIZ); // the joystick must be in NEUTRAL 
  joy_y = analogRead(JOYSTICK_VERT);  // POSITION AT THIS STAGE!
  /* determine the minimum x and y remap values, so that we map the default
     reading of the x and y position of the joystick to exactly 0; this formula
     was mathematically determined from the Arduino map function's source code,
     casting here is necessary to avoid overflow when multiplying by 1000. */
  joy_min_x = (((int32_t) joy_x) * JOY_REMAP_MAX) / (joy_x - VOLT_MAX);
  joy_min_y = (((int32_t) joy_y) * JOY_REMAP_MAX) / (joy_y - VOLT_MAX);

  
}

//*****************************************************************************
//                        Sec0.5: Arduino Loop Procedure
//*****************************************************************************

void loop()
{
  // read joystick input regardless of mode
  joy_x = analogRead(JOYSTICK_HORIZ);
  joy_y = analogRead(JOYSTICK_VERT);
  // remap joy_x and joy_y to values in the range ~-1000 - 1000
  joy_x = map((joy_x), VOLT_MIN, VOLT_MAX, joy_min_x, JOY_REMAP_MAX);
  joy_y = map((joy_y), VOLT_MIN, VOLT_MAX, joy_min_y, JOY_REMAP_MAX);

  if (game_state == SETUP_MODE){
    // Fill the red checker array
    for (uint8_t i = 1; i < NUM_TILES; i++){
      tile_array[i].has_checker = 0;
    }
    for (uint8_t i = 1; i < CHECKERS_PER_SIDE; i++) {	
      uint8_t x_ti = (2*(i%4) + (((i/4) + 1)%2)); // fancy maths
      uint8_t y_ti = i/4;
      red_checkers[i].x_tile = x_ti;
      red_checkers[i].y_tile = y_ti;
      // record that the proper tiles contain these checkers
      tile_array[coord_to_tile(x_ti, y_ti)].has_checker = TURN_RED;
      tile_array[coord_to_tile(x_ti, y_ti)].checker_num = i;
    }
    // red_checkers[0].x_tile = 1;
    // red_checkers[0].y_tile = 4;
    // tile_array[coord_to_tile(red_checkers[0].x_tile, red_checkers[0].y_tile)].has_checker = TURN_RED;
    // tile_array[coord_to_tile(red_checkers[0].x_tile, red_checkers[0].y_tile)].checker_num = 0;
    // draw_tile(tile_array, red_checkers, blue_checkers, coord_to_tile(red_checkers[0].x_tile, red_checkers[0].y_tile));

    // Fill the blue checker array
    for (uint8_t i = 0; i < CHECKERS_PER_SIDE; i++) {
      // x_ti, y_ti first declared above
      uint8_t x_ti = (2*(i%4) + ((i/4)%2));
      uint8_t y_ti = (i/4) + 5;
      blue_checkers[i].x_tile = x_ti;
      blue_checkers[i].y_tile = y_ti;
      // record that the proper tiles contain these checkers
      tile_array[coord_to_tile(x_ti, y_ti)].has_checker = TURN_BLUE;
      tile_array[coord_to_tile(x_ti, y_ti)].checker_num = i;
    }
    for (uint8_t i = 1; i < 24; i = i + 2) { // draw all the checker tiles
      draw_tile(tile_array, red_checkers, blue_checkers, i - (i/8)%2);
      draw_tile(tile_array, red_checkers, blue_checkers, (i + 40) -
		((i+40)/8)%2); // works, don't know why
    }
    
    player_checkers = &blue_checkers[0];
    highlight_tile(tile_highlighted, player_turn, tile_selected);

    // for (uint8_t i=0; i < 8; i++){
    //   for (uint8_t j=0; j < 8; j++) {
    // 	highlight_tile(j , i, 'b', 1);
    // 	delay(200);
    // 	draw_tile(tile_array, red_checkers,
    // 		  blue_checkers, coord_to_tile(j, i));

    //   }
    // }
    // for (uint8_t i = 1; i < 13; i++){
    //   populate_graveyard(i, 'r');
    //   delay(200);
    //   populate_graveyard(i, 'b');
    // }
    // turn_indicator('r');
    // delay(10000);
    // turn_indicator('b');
    game_state = PLAY_MODE;
    tile_array[0].has_checker = 42;
  }


  else if (game_state == PLAY_MODE){
    if (recompute_force_jumps){
      // compute the moves

      for (int i = 0; i < CHECKERS_PER_SIDE; i++){
	for (int j = 0; j < 4; j++){
	  player_checkers[i].moves[j] = 0;
	  player_checkers[i].jumps[j] = 0;
	}
      }
      no_fjumps = compute_moves(tile_array, player_checkers, 
				(-1) * player_turn);
      recompute_force_jumps = 0;
    }
    // set the highlight movement delay time
    if (abs(joy_x) > 900 || abs(joy_y) > 900){ delay_time = 50; }
  else { delay_time = 220; }

  joy_x = joy_x / 400; // in range [-1, 1]
  joy_y = joy_y / 400;
  if ((joy_x != 0 || joy_y != 0) && !tile_selected) {
    draw_tile(tile_array, red_checkers, blue_checkers, 
	      tile_highlighted);
    
    if (joy_x <= -1) { // we've moved the joystick left
      if (tile_highlighted % 8) {tile_highlighted--;}
    }
    if (joy_x >= 1) { // we've moved the joystick right
      if ((tile_highlighted + 1) % 8) {tile_highlighted++;}
    }
    if (joy_y <= -1) { // we've moved the joystick up
      if (tile_highlighted/8) {tile_highlighted -= 8;}
    }
    if (joy_y >= 1) { // we've moved the joystick down
      if ((tile_highlighted/8) < 7) {tile_highlighted+=8;}
    }
    if (!tile_selected){
      highlight_tile(tile_highlighted, player_turn, tile_selected);
      subtile_highlighted = tile_highlighted;
      delay(delay_time);
    }
  }
  else if ((joy_x !=0 || joy_y != 0) && tile_selected){
    draw_tile(tile_array, red_checkers, blue_checkers, 
	      subtile_highlighted);
    if (joy_x <= -1) { // we've moved the joystick left
      if (subtile_highlighted % 8) {subtile_highlighted--;}
    }
    if (joy_x >= 1) { // we've moved the joystick right
      if ((subtile_highlighted + 1) % 8) {subtile_highlighted++;}
    }
    if (joy_y <= -1) { // we've moved the joystick up
      if (subtile_highlighted/8) {subtile_highlighted -= 8;}
    }
    if (joy_y >= 1) { // we've moved the joystick down
      if ((subtile_highlighted/8) < 7) {subtile_highlighted+=8;}
    }
    if (tile_selected){
      if (no_fjumps) { 
	highlight_moves(tile_array, tile_highlighted, player_checkers);
      }
      else {
	highlight_jumps(tile_array, tile_highlighted, player_checkers);
      }
      highlight_tile(subtile_highlighted, player_turn, 0);
      highlight_tile(tile_highlighted, player_turn, tile_selected);
      delay(delay_time);
    }
  }

  // Serial.print("no_fjumps: ");
  // Serial.println(no_fjumps);
  // Serial.print("tile has checker: ");
  // Serial.println(tile_array[33].has_checker);
  // Serial.print("tile has checker no.: ");
  // Serial.println(tile_array[33].checker_num);
  // delay(1000);
  if (digitalRead(JOYSTICK_BUTTON) == LOW){
    delay(500);
    if (tile_selected) {
      move_ok = selection_matches_move(subtile_highlighted, &active_checker, 
				       no_fjumps);
      if (move_ok){
	move_piece(tile_array, &active_checker, tile_highlighted, 
		   subtile_highlighted);
      }
      else { tile_selected = 0; }

      draw_tile(tile_array, red_checkers, blue_checkers, tile_highlighted);
      for (uint8_t i = 0; i < 4; i++){
	draw_tile(tile_array, red_checkers, blue_checkers, 
		  active_checker.moves[i]);
	draw_tile(tile_array, red_checkers, blue_checkers, 
		  active_checker.jumps[i]);
      }

	tile_highlighted = subtile_highlighted;
      }
    active_checker = 
      player_checkers[tile_array[tile_highlighted].checker_num];
    checker_owned = (tile_array[tile_highlighted].has_checker == player_turn);
    checker_must_jump = (player_checkers[tile_array[
				     tile_highlighted].checker_num].must_jump);
    recompute_force_jumps = 1;

    
    if ((!tile_selected) && no_fjumps && check_can_move(&active_checker)){
      // check whether the piece we selected is on our team
      if (checker_owned){
	tile_selected = 1;
	highlight_tile(tile_highlighted, player_turn, tile_selected);
	highlight_moves(tile_array, tile_highlighted, player_checkers);
      }
    }
    else if (!no_fjumps){
      // check whether the selected piece must jump
      if (checker_owned && checker_must_jump) {
	tile_selected = 1;
	highlight_tile(tile_highlighted, player_turn, tile_selected);
	highlight_jumps(tile_array, tile_highlighted, player_checkers);
      }
    }

  }
  else {current_state_a = 0;}
  }


  else if (game_state == GAMEOVER_MODE){}
}

