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
#include "TimerThree.h"
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

#define DEBUG_BUTTON 10

// Sub0.15: mode mapping
#define TURN_RED 1
#define TURN_BLUE -1
#define SETUP_MODE 0
#define PLAY_MODE 1
#define GAMEOVER_MODE 2
#define HIGHLIGHT_MOVES 3
#define HIGHLIGHT_JUMPS 4
#define BOUNCE_PERIOD 500000
#define TILE_MOVEMENT 0
#define SUBTILE_MOVEMENT 1
#define DEFAULT_TILE 36


// Sub0.16: color mapping
#define TILE_HIGHLIGHT 0xFFF7 // almost white
#define RED_HIGHLIGHT ST7735_RED
#define BLUE_HIGHLIGHT ST7735_BLUE
#define MOVE_HIGHLIGHT ST7735_GREEN
#define JUMP_HIGHLIGHT ST7735_MAGENTA

//*****************************************************************************
//                   Sec0.2: Non-Constant Globals and Cache Data     
//*****************************************************************************

// Sub0.20: checker arrays
Checker red_checkers[CHECKERS_PER_SIDE];  // the array of red checkers
Checker blue_checkers[CHECKERS_PER_SIDE]; // the array of blue checkers

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
uint8_t tile_highlighted = DEFAULT_TILE;
uint8_t subtile_highlighted;
uint8_t tile_selected = 0; // whether we have selected a tile
uint8_t subtile_selected = 0;
uint16_t delay_time = 100;
bool checker_owned;
bool checker_must_jump;

// Sub?: game state
uint8_t game_state = SETUP_MODE; // we need to set up the board
uint8_t red_dead = 0;
uint8_t blue_dead = 0;

uint8_t recompute_moves = 1;
uint8_t cursor_mode = TILE_MOVEMENT;
uint8_t no_fjumps = 1;

uint8_t bouncer = 0;
uint8_t signal_redraw = 0;

// Sub?: turn-based vars
uint8_t* player_dead = &red_dead;
Checker* active_checker;
Checker* player_checkers;
int8_t player_turn = TURN_RED;



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
void highlight_tile(uint8_t tile_num, int8_t mode)
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

  if (mode == TURN_RED) { color = RED_HIGHLIGHT; }
  else if (mode == TURN_BLUE) { color = BLUE_HIGHLIGHT; }
  else { color = mode; }

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
void change_turn()
{
  /*
    this function is called to indicate on the lcd display whose turn it is,
    and does so by coloring the border their respective color, where:

    turn: whose turn we would like to indicate on screen

    uses globals: cbr_image, cbb_image, tft
   */
  player_turn = (-1) * player_turn;

  if (player_turn == TURN_RED){
    player_checkers = red_checkers;
    player_dead = &red_dead;
    lcd_image_draw(&cbr_image, &tft, 0, 0, 0, 0, SCREEN_WIDTH, BORDER_WIDTH);
    lcd_image_draw(&cbr_image, &tft, 0, 0, 0, 0, BORDER_WIDTH, SCREEN_WIDTH);
    lcd_image_draw(&cbr_image, &tft, SCREEN_WIDTH - BORDER_WIDTH, 0, 
		   SCREEN_WIDTH - BORDER_WIDTH, 0, BORDER_WIDTH, SCREEN_WIDTH);
    lcd_image_draw(&cbr_image, &tft, 0, SCREEN_WIDTH - BORDER_WIDTH, 0, 
		   SCREEN_WIDTH - BORDER_WIDTH, SCREEN_WIDTH, BORDER_WIDTH);
  }
  else {
    player_checkers = blue_checkers;
    player_dead = &blue_dead;
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
    if (checkers[i].in_play){
      compute_checker_moves(tile_array, &checkers[i], opp_color);
      compute_checker_jumps(tile_array, &checkers[i], opp_color);
      if (checkers[i].must_jump){
	no_fjumps = 0;
      }
    }
  }
  return no_fjumps;
}

void highlight_moves(Checker* active_checker)
{
  for (uint8_t i = 0; i < 4; i++){
    if (active_checker->moves[i] != 64){
      highlight_tile(active_checker->moves[i], MOVE_HIGHLIGHT);
    }
  }
}
void highlight_jumps(Checker* active_checker)
{
  for (uint8_t i = 0; i < 4; i++){
    if (active_checker->jumps[i] != 64){
      highlight_tile(active_checker->jumps[i], JUMP_HIGHLIGHT);
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
  return matches;
}

uint8_t check_can_move(Checker* active_checker)
{
  uint8_t matches = 0;

  for (uint8_t i = 0; i < 4; i++){

    if (active_checker->moves[i] != 64){
      matches = 1;
    }
  }

  return matches;
}	

uint8_t check_must_jump(Checker* active_checker)
{
  uint8_t matches = 0;
  for (uint8_t i = 0; i < 4; i++){
    if (active_checker->jumps[i] != 64){
      matches = 1;
    }
  }
  return matches;
}		    

void clear_draw(Tile* tile_array, Checker* active_checker,
		Checker* red_checkers, Checker* blue_checkers, 
		uint8_t active_tile, uint8_t destination_tile) 
{
  uint8_t rm_tile = (active_tile + destination_tile) / 2;

  draw_tile(tile_array, red_checkers, blue_checkers, active_tile);
  draw_tile(tile_array, red_checkers, blue_checkers, destination_tile);
  draw_tile(tile_array, red_checkers, blue_checkers, rm_tile);
  for (uint8_t i = 0; i < 4; i++){
    if (active_checker->moves[i] != 0){
      draw_tile(tile_array, red_checkers, blue_checkers,
		active_checker->moves[i]);
    }
    if (active_checker->jumps[i] != 0) {
      draw_tile(tile_array, red_checkers, blue_checkers, 
		active_checker->jumps[i]);
    }
  }
}

void move_checker(Tile* tile_array, Checker* active_checker, 
		  uint8_t active_tile, uint8_t destination_tile, 
		  Checker* red_checkers, Checker* blue_checkers)
{
  tile_array[destination_tile].has_checker = 
    tile_array[active_tile].has_checker;
  tile_array[destination_tile].checker_num = 
    tile_array[active_tile].checker_num;

  tile_array[active_tile].has_checker = 0;
  tile_array[active_tile].checker_num = 13;

  uint8_t* x_y = tile_to_coord(destination_tile);

  active_checker->x_tile = x_y[0];
  active_checker->y_tile = x_y[1];
  Serial.print("x_y[1]: "); Serial.println(x_y[1]);
  if(x_y[1] == 0 || x_y[1] == 7){
    active_checker->is_kinged = 1;
  }

   clear_draw(tile_array, active_checker, red_checkers, blue_checkers,
	     active_tile, destination_tile);

}
void jump_checker(Tile* tile_array, Checker* active_checker, 
		  uint8_t active_tile, uint8_t destination_tile, 
		  Checker* red_checker, Checker* blue_checker,
		  uint8_t* num_dead, uint8_t turn)
{
  tile_array[destination_tile].has_checker = 
    tile_array[active_tile].has_checker;
  tile_array[destination_tile].checker_num = 
    tile_array[active_tile].checker_num;

  tile_array[active_tile].has_checker = 0;
  tile_array[active_tile].checker_num = 13;
  
  uint8_t rm_tile =  (active_tile + destination_tile) / 2;

  uint8_t* x_y = tile_to_coord(destination_tile);
  active_checker->x_tile = x_y[0];
  active_checker->y_tile = x_y[1];


  if (turn == TURN_RED) {
    blue_checkers[tile_array[rm_tile].checker_num].in_play = 0;
    for (uint8_t i = 0; i < 4; i++){
      blue_checker[tile_array[rm_tile].checker_num].moves[i] = 64;
      blue_checker[(tile_array[rm_tile].checker_num)].jumps[i] = 64;
    }
  }
  else {
    red_checker[tile_array[rm_tile].checker_num].in_play = 0;
    for (uint8_t i = 0; i < 4; i++){
      red_checker[tile_array[rm_tile].checker_num].moves[i] = 64;
      red_checker[tile_array[rm_tile].checker_num].jumps[i] = 64;
    }
  }
  tile_array[rm_tile].has_checker = 0;
  tile_array[rm_tile].checker_num = 13;

  clear_draw(tile_array, active_checker, red_checker, blue_checker,
	     active_tile, destination_tile);
  // clear_draw(tile_array, active_checker, red_checkers, blue_checkers, 
  // 	     active_tile, destination_tile);

  // Serial.print(*num_dead); Serial.print(" "); 
  (*num_dead) = (*num_dead) + 1;
  // Serial.println(*num_dead);
  populate_graveyard(*num_dead, turn);
}



void bouncer_reset()
{
  if(bouncer < 3){
    bouncer++;
  }
}

uint8_t modify_tile_select(int joy_x, int joy_y, uint8_t* tile_highlighted, 
			     uint8_t tile_selected)
{
  if (joy_x <= -1) { // we've moved the joystick left
    if ((*tile_highlighted) % 8) {(*tile_highlighted)--;}
  }
  if (joy_x >= 1) { // we've moved the joystick right
    if (((*tile_highlighted) + 1) % 8) {(*tile_highlighted)++;}
  }
  if (joy_y <= -1) { // we've moved the joystick up
    if ((*tile_highlighted/8)) {(*tile_highlighted) -= 8;}
  }
  if (joy_y >= 1) { // we've moved the joystick down
    if (((*tile_highlighted)/8) < 7) {(*tile_highlighted)+=8;}
  }
}

uint8_t player_piece_on_tile(Tile* tile_array, uint8_t tile_highlighted, 
			     int8_t current_turn)
{
  return (tile_array[tile_highlighted].has_checker == current_turn);
}

void print_all_data(Tile* tile_array, Checker* red_checkers, Checker* blue_checkers,
		    Checker* player_checkers){
  Serial.println();
  Serial.println("has_checker board:");
  Serial.println("**********");
  for(int i = 0; i < NUM_TILES; i++){
    if( !(i%8) ){
      Serial.print("*");
    }
    if(tile_array[i].has_checker == 0){
      Serial.print("0");
    }
    else if(tile_array[i].has_checker == 1){
      Serial.print("R");
    }
    else if(tile_array[i].has_checker == -1){
      Serial.print("B");
    }
    else {Serial.print(tile_array[i].has_checker);}
    if( !((i+1) % 8)){
      Serial.println("*");
    }
  }
  Serial.println("**********");
  Serial.println();
  Serial.println("checker_num board:");
  Serial.println("******************");
  for(int i = 0; i < NUM_TILES; i++){
    if( !(i%8) ){
      Serial.print("*");
    }
    if(tile_array[i].checker_num < 10){
      Serial.print(" ");
      Serial.print(tile_array[i].checker_num);
    }
    else {Serial.print(tile_array[i].checker_num);}
    if( !((i+1) % 8)){
      Serial.println("*");
    }
  }
  Serial.println("******************");
  Serial.println();

  Serial.println("Red checker data:");
  Serial.println("   X: Y: King: In: MJ: Jumps:          Moves:");
  Serial.println();
  for(int i=0; i < CHECKERS_PER_SIDE; i++){
    Serial.print("#"); Serial.print(i); Serial.print(":"); Serial.print(" ");
    if(i < 10){ Serial.print(" ");}
    Serial.print(red_checkers[i].x_tile); Serial.print(" ");
    if(red_checkers[i].x_tile < 10){Serial.print(" ");}
    Serial.print(red_checkers[i].y_tile); Serial.print("    ");
    if(red_checkers[i].x_tile < 10){Serial.print(" ");}
    Serial.print(red_checkers[i].is_kinged); Serial.print("   ");
    Serial.print(red_checkers[i].in_play); Serial.print("   ");
    Serial.print(red_checkers[i].must_jump); Serial.print("      ");
    for(int j = 0; j < 4; j++){
      Serial.print(red_checkers[i].jumps[j]); Serial.print(", ");
      if(red_checkers[i].jumps[j] < 10){ Serial.print(" ");}
    }
    for(int j = 0; j < 4; j++){
      Serial.print(red_checkers[i].moves[j]); Serial.print(", ");
      if(red_checkers[i].moves[j] < 10){ Serial.print(" ");}
    }
    Serial.println();
  }
  Serial.println();
  Serial.println();
  Serial.println("Blue checker data:");
  Serial.println("   X: Y: King: In: MJ: Jumps:          Moves:");
  Serial.println();
  for(int i=0; i < CHECKERS_PER_SIDE; i++){
    Serial.print("#"); Serial.print(i); Serial.print(":"); Serial.print(" ");
    if(i < 10){ Serial.print(" ");}
    Serial.print(blue_checkers[i].x_tile); Serial.print(" ");
    if(blue_checkers[i].x_tile < 10){Serial.print(" ");}
    Serial.print(blue_checkers[i].y_tile); Serial.print("    ");
    if(blue_checkers[i].x_tile < 10){Serial.print(" ");}
    Serial.print(blue_checkers[i].is_kinged); Serial.print("   ");
    Serial.print(blue_checkers[i].in_play); Serial.print("   ");
    Serial.print(blue_checkers[i].must_jump); Serial.print("      ");
    for(int j = 0; j < 4; j++){
      Serial.print(blue_checkers[i].jumps[j]); Serial.print(", ");
      if(blue_checkers[i].jumps[j] < 10){ Serial.print(" ");}
    }
    for(int j = 0; j < 4; j++){
      Serial.print(blue_checkers[i].moves[j]); Serial.print(", ");
      if(blue_checkers[i].moves[j] < 10){ Serial.print(" ");} 
    }
    Serial.println();
  }
  Serial.println();
  Serial.println();
}

// Prints the board to serial-mon for debugging purposes
void print_board_data(Tile* tile_array){
  Serial.println("**********");
  for(int i = 0; i < NUM_TILES; i++){
    if( !(i%8) ){
      Serial.print("*");
    }
    if(tile_array[i].has_checker == 0){
      Serial.print("0");
    }
    else if(tile_array[i].has_checker == 1){
      Serial.print("R");
    }
    else if(tile_array[i].has_checker == -1){
      Serial.print("B");
    }
    else {Serial.print("0");}
    if( !((i+1) % 8)){
      Serial.println("*");
    }
  }
  Serial.println("**********");
  Serial.println(no_fjumps);

  Serial.println("RED:");
  for (int i = 0 ; i < CHECKERS_PER_SIDE; i++){
    Serial.print("checker["); Serial.print(i); Serial.print("]s moves: ");
    for (int j = 0; j < 4; j++) {
      Serial.print(red_checkers[i].moves[j]); Serial.print(", ");
    }
    Serial.print("  location: ("); Serial.print(red_checkers[i].x_tile);
    Serial.print(", "); Serial.print(red_checkers[i].y_tile); 
    Serial.print(" ) ");
    Serial.println();
  }
  for (int i = 0 ; i < CHECKERS_PER_SIDE; i++){

    Serial.println("RED:");
    Serial.print("checker["); Serial.print(i); Serial.print("]s jumps: ");
    for (int j = 0; j < 4; j++) {
      Serial.print(red_checkers[i].jumps[j]); Serial.print(", ");
    }
    Serial.print("  location: ("); Serial.print(red_checkers[i].x_tile);
    Serial.print(", "); Serial.print(red_checkers[i].y_tile); 
    Serial.print(" ) ");
    Serial.println();
  }
  for (int i = 0 ; i < CHECKERS_PER_SIDE; i++){
    Serial.println("BLUE:");
    Serial.print("checker["); Serial.print(i); Serial.print("]s moves: ");
    for (int j = 0; j < 4; j++) {
      Serial.print(blue_checkers[i].moves[j]); Serial.print(", ");
    }
    Serial.print("  location: ("); Serial.print(blue_checkers[i].x_tile);
    Serial.print(", "); Serial.print(blue_checkers[i].y_tile); 
    Serial.print(" ) ");
    Serial.println();
  }
  for (int i = 0 ; i < CHECKERS_PER_SIDE; i++){

    Serial.println();
    Serial.println("BLUE:");
    Serial.print("checker["); Serial.print(i); Serial.print("]s jumps: ");
    for (int j = 0; j < 4; j++) {
      Serial.print(blue_checkers[i].jumps[j]); Serial.print(", ");
    }
    Serial.print("  location: ("); Serial.print(blue_checkers[i].x_tile);
    Serial.print(", "); Serial.print(blue_checkers[i].y_tile); 
    Serial.print(" ) ");
    Serial.println();
    Serial.println();
  }
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

  pinMode(DEBUG_BUTTON, INPUT);
  digitalWrite(DEBUG_BUTTON, HIGH);

  // Sub0.44: calibrate the joystick
  joy_x = analogRead(JOYSTICK_HORIZ); // the joystick must be in NEUTRAL 
  joy_y = analogRead(JOYSTICK_VERT);  // POSITION AT THIS STAGE!
  /* determine the minimum x and y remap values, so that we map the default
     reading of the x and y position of the joystick to exactly 0; this formula
     was mathematically determined from the Arduino map function's source code,
     casting here is necessary to avoid overflow when multiplying by 1000. */
  joy_min_x = (((int32_t) joy_x) * JOY_REMAP_MAX) / (joy_x - VOLT_MAX);
  joy_min_y = (((int32_t) joy_y) * JOY_REMAP_MAX) / (joy_y - VOLT_MAX);

  Timer3.initialize();
  Timer3.attachInterrupt(bouncer_reset, BOUNCE_PERIOD);
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
    for (uint8_t i = 0; i < CHECKERS_PER_SIDE; i++) {	
      uint8_t x_ti = (2*(i%4) + (((i/4) + 1)%2)); // fancy maths
      uint8_t y_ti = i/4;
      red_checkers[i].x_tile = x_ti;
      red_checkers[i].y_tile = y_ti;
      red_checkers[i].is_kinged = 0;
      red_checkers[i].in_play = 1;
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
      blue_checkers[i].is_kinged = 0;
      blue_checkers[i].in_play = 1;
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

    // Serial.println(*player_dead);
    // delay(1000);

  }


  else if (game_state == PLAY_MODE){
    if (recompute_moves){
      change_turn();
      highlight_tile(tile_highlighted, player_turn);

      for (int i = 0; i < CHECKERS_PER_SIDE; i++){
	// assume all previous moves are now invalid
	player_checkers[i].must_jump = 0;
	for (int j = 0; j < 4; j++){
	  player_checkers[i].moves[j] = 64; // 64 is not a tile, void status
	  player_checkers[i].jumps[j] = 64;
	}
      }
      
      no_fjumps = compute_moves(tile_array, player_checkers, 
				(-1) * player_turn);
      recompute_moves = 0;

    }
    // set the highlight movement delay time
    if (abs(joy_x) > 900 || abs(joy_y) > 900){ delay_time = 50; }
    else { delay_time = 220; }
    
    joy_x = joy_x / 400; // in range [-1, 1]
    joy_y = joy_y / 400;

    if ((joy_x != 0 || joy_y != 0) && !cursor_mode) { // moved, not selecting
      // modify the primary tile highlight
      draw_tile(tile_array, red_checkers, blue_checkers, tile_highlighted);
      modify_tile_select(joy_x, joy_y, &tile_highlighted, tile_selected);

      // redraw certain tiles
      highlight_tile(tile_highlighted, player_turn);
      delay(delay_time);
    }

    else if ((joy_x !=0 || joy_y != 0) && cursor_mode){ // moved, selecting
      // modify the secondary tile highlight
      draw_tile(tile_array, red_checkers, blue_checkers, subtile_highlighted);
      modify_tile_select(joy_x, joy_y, &subtile_highlighted, subtile_selected);

      if (no_fjumps){ highlight_moves(active_checker); }
      else { highlight_jumps(active_checker); }
      highlight_tile(subtile_highlighted, player_turn);
      highlight_tile(tile_highlighted, TILE_HIGHLIGHT);
      delay(delay_time);

    }

    if (digitalRead(JOYSTICK_BUTTON) == LOW && bouncer > 1){
      bouncer = 0; // debounce
      if (cursor_mode == TILE_MOVEMENT) {
	if (player_piece_on_tile(tile_array, tile_highlighted, player_turn)){
	  // set active checker to the one we're selecting
	  active_checker = 
	 (Checker*) &player_checkers[tile_array[tile_highlighted].checker_num];

	  if (no_fjumps) {
	    // check whether piece can move
	    if (check_can_move(active_checker)) {
	      // select the checker and highlight its moves
	      highlight_moves(active_checker);
	      highlight_tile(tile_highlighted, TILE_HIGHLIGHT);

	      cursor_mode = SUBTILE_MOVEMENT;
	      subtile_highlighted = tile_highlighted;
	    }
	  }
	  else {
	    // check whether piece must jump
	    if (check_must_jump(active_checker)) {
	      // select the checker and highlight its jumps
	      highlight_jumps(active_checker);
	      highlight_tile(tile_highlighted, TILE_HIGHLIGHT);
	      signal_redraw = 1;

	      cursor_mode = SUBTILE_MOVEMENT;
	      subtile_highlighted = tile_highlighted;
	    }
	  }
	} // end piece is ours if
      } // end tile movement if
      else if (cursor_mode == SUBTILE_MOVEMENT){
	if (no_fjumps){
	  if (selection_matches_move(subtile_highlighted, active_checker, 
				     no_fjumps)) {
	    // move checker
	    move_checker(tile_array, active_checker, 
			 tile_highlighted, subtile_highlighted, 
			 red_checkers, blue_checkers);

	    // tile_highlighted = DEFAULT_TILE;
	    recompute_moves = 1;
	    highlight_tile(tile_highlighted, player_turn);
	  }
	  else {
	    clear_draw(tile_array, active_checker, red_checkers, blue_checkers,
		       tile_highlighted, subtile_highlighted);
	    highlight_tile(tile_highlighted, player_turn);

	  }
	  cursor_mode = TILE_MOVEMENT;
	}
	else {
	  if(selection_matches_move(subtile_highlighted, active_checker, 
				    no_fjumps)) {
	    // jump checker
	    jump_checker(tile_array, active_checker, tile_highlighted,
			 subtile_highlighted, red_checkers, blue_checkers,
			 player_dead, player_turn);

	    recompute_moves = 1;
	    // tile_highlighted = DEFAULT_TILE;
	    highlight_tile(tile_highlighted, player_turn);
	    
	  }
	  else {
	    clear_draw(tile_array, active_checker, red_checkers, blue_checkers,
		       tile_highlighted, subtile_highlighted);
	    
	  }
	  cursor_mode = TILE_MOVEMENT;
	  highlight_tile(tile_highlighted, player_turn);

	}
      }
    } // end button press if

    else if (digitalRead(DEBUG_BUTTON) == LOW){print_all_data(tile_array,
			    red_checkers, blue_checkers, player_checkers);}
  }
}

