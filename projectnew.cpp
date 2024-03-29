/*
  Table of Contents
  
  Section:
  Sec0.0: Included Files and libraries                  
  Sec0.1: Global Variables and Constants
    Sub0.100: lcd screen wiring
    Sub0.101: screen constants
    Sub0.102: checkers settings
    Sub0.103: joystick range information
    Sub0.104: joystick pins
    Sub0.105: turn mapping
    Sub0.106: game state mapping
    Sub0.107: debounce
    Sub0.108: cursor mode mapping
    Sub0.109: tile settings
    Sub0.110: color mapping
    Sub0.111: note mapping
    Sub0.112: sound playing
    Sub0.113: optional pins
  Sec0.2: Non-Constant Globals and Cache Data
    Sub0.200: checker player variables
    Sub0.201: tile array
    Sub0.202: tft object
    Sub0.203: card object
    Sub0.204: lcd image objects
    Sub0.205: joystick variables
    Sub0.206: tile highlighting
    Sub0.207: game states
    Sub0.208: active player variables and pointers
    Sub0.209: debounce
  Sec0.3: Functions
    Sub0.300: tile -> coordinate // coordinate -> tile maps
    Sub0.301: drawing procedures
    Sub0.302: move and jump computing
    Sub0.303: highlighting
    Sub0.304: move & jump verification
    Sub0.305: checker jumping & movement
    Sub0.306: debounce reset
    Sub0.307: joystick tile manipulation
    Sub0.308: nicer boolean functions
    Sub0.309: debounce reset
    Sub0.310: sounds & music
    Sub0.311: debug procedures
  Sec0.4: Arduino Setup Procedure
    Sub0.400: serial monitor & sd card preliminaries
    Sub0.401: drawing the checker board
    Sub0.402: set up pins
    Sub0.403: calibrate the joystick
    Sub0.404: initialize time-based interrupt
  Sec0.5: Arduino Loop Procedure
    Sub0.500: joystick reading & calibration
    Sub0.501: game setup
    Sub0.502: changing turns
    Sub0.503: reading joystick movement
    Sub0.504: reading button presses (contains subs 505, 506, 507)
    Sub0.505: checker selection
    Sub0.506: move selection
    Sub0.507: jump selection
    Sub0.508: debug prompt
 */

//****************************************************************************
//                     Sec0.0: Included Files and Libraries
//****************************************************************************

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
#include "projectnew.h"

//****************************************************************************
//                    Sec0.1: Global Variables and Constants
//****************************************************************************
// Sub0.100: lcd screen wiring
#define SD_CS 5
#define TFT_CS 6
#define TFT_DC 7
#define TFT_RST 8

// Sub0.101: screen constants
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 160
#define BORDER_WIDTH 4 // Border surrounding the playable area
#define CBSQUARE_SIZE 8 // standard 8x8 checker board

#define GRAVSTART_BLUEX 18 // the x, y start positions of the checkers in 
#define GRAVSTART_REDX 66  // the graveyard
#define GRAVSTART_Y 128 // both start at the same horizontal level
#define GRAV_ROWSEP 1 // the vertical separation between each row in graveyard

#define WINSTART_X 18
#define WINSTART_Y 21
#define WIN_INCREMENT 30
#define RED_OFFSET 15

// Sub0.102: checkers settings
const uint8_t CHECKERS_PER_SIDE = 12; // 12 pieces per side, as per game rules
const uint8_t NUM_TILES = 64; // standard 8x8 board
#define POSSIBLE_MOVES 4
#define TILE_SIZE ((128-8)/8)// tile width and height in pixels
#define GRAV_PIECEWIDTH 11 // the width/height of a graveyard checker piece
#define GRAV_PIECEHEIGHT 10

// Sub0.103: joystick range information
#define VOLT_MIN 0          // 0 volts 
#define VOLT_MAX 1023       // 5 volts
#define JOY_REMAP_MAX 1000  // For remapping 0-1023 to ~-1000 - 1000

// Sub0.104: joystick pins
#define JOYSTICK_HORIZ 0    // Analog input A0 - horizontal
#define JOYSTICK_VERT  1    // Analog input A1 - vertical
#define JOYSTICK_BUTTON 9   // Digitial input pin 9 for the button

#define SPEAKER_PIN  11

// Sub0.105: turn mapping
#define TURN_RED 1
#define TURN_BLUE -1

// Sub0.106: game state mapping
#define SETUP_MODE 0
#define PLAY_MODE 1
#define GAMEOVER_MODE 2

// Sub0.107: debounce
#define BOUNCE_PERIOD 500000

// Sub0.108: cursor mode mapping
#define TILE_MOVEMENT 0
#define SUBTILE_MOVEMENT 1

// Sub0.109: tile settings
#define DEFAULT_TILE 36
#define VOID_TILE 64

// Sub0.110: color mapping
#define TILE_HIGHLIGHT 0xFFF7 // almost white
#define RED_HIGHLIGHT ST7735_RED
#define BLUE_HIGHLIGHT ST7735_BLUE
#define MOVE_HIGHLIGHT ST7735_GREEN
#define JUMP_HIGHLIGHT 0xfb20 // orange

// Sub0.111: note mapping
#define NOTE_EB3 156
#define NOTE_GB3 185
#define NOTE_G3  196
#define NOTE_AB3 208
#define NOTE_B3  247
#define NOTE_EB4 311
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_EB5 622
#define NOTE_DB5 554
#define REST 0

// Sub0.112: sound playing
#define DUR_MOVE 20
#define DUR_JUMP 50
#define DUR_NOTE1 50
#define DUR_NOTE2 75
#define DUR_NOTE3 50
#define DUR_NOTE4 200
#define NOTE_DELAY 100
#define QUARTER_NOTE 400
#define DOTTED_HALF 1200
#define TRIPLET_EIGHTH 133

// Sub0.113: optional pins
#define DEBUG_BUTTON 10

//****************************************************************************
//                   Sec0.2: Non-Constant Globals and Cache Data     
//****************************************************************************

// Sub0.200: checkers player variables
Checker red_checkers[CHECKERS_PER_SIDE];  // the array of red checkers
Checker blue_checkers[CHECKERS_PER_SIDE]; // the array of blue checkers
uint8_t red_dead = 0;
uint8_t blue_dead = 0;
uint8_t rm_tile;

// Sub0.201: tile array
Tile tile_array[NUM_TILES];

// Sub0.202: tft object
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Sub0.203: card object
Sd2Card card;

// Sub0.204: lcd image objects
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

// Sub0.205: joystick variables
int joy_x;           // x and 
int joy_y;           // y positions of the joystick
int joy_min_x;       // remap minima for the x and
int joy_min_y;       // y readings from the joystick
uint16_t joy_delay_time = 100; // depends on intensity of joystick movement


// Sub0.206: tile highlighting
uint8_t tile_highlighted = DEFAULT_TILE;
uint8_t subtile_highlighted;
uint8_t tile_selected = 0; // whether we have selected a tile
uint8_t subtile_selected = 0;
uint8_t checker_locked = 0; // player cannot unselect their checker

// Sub0.207: game states
uint8_t game_state = SETUP_MODE; // we need to set up the board
uint8_t cursor_mode = TILE_MOVEMENT;
uint8_t turn_change = 1; // compute them to start
uint8_t no_fjumps = 1;
uint8_t no_moves = 0;
uint8_t signal_redraw = 0;
int16_t player_turn = TURN_RED; //16-bit necessary for tile_highlight function

// Sub0.208: active player variable pointers
uint8_t* player_dead = &red_dead;
Checker* active_checker;
Checker* player_checkers;

// Sub0.209: debounce
uint8_t bouncer = 0;


//****************************************************************************
//                             Sec0.3: Functions     
//****************************************************************************



// Sub0.300: tile -> coordinate // coordinate -> tile maps

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


// Sub0.301: drawing procedures

void draw_tile(Tile* tile_array, Checker* red_checkers, 
	       Checker* blue_checkers, uint8_t tile_index)
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

void clear_draw(Tile* tile_array, Checker* active_checker,
		Checker* red_checkers, Checker* blue_checkers, 
		uint8_t active_tile, uint8_t destination_tile) 
{
  /*
    this procedure essentially draws over everything that could have changed
    using the above draw_tile function; there are several different modes of 
    highlighting, yet having all clearing operate in one procedure does not 
    hamper the speed of the program since it is a  turn-based game and 
    constant re-drawing is not necessary, where:
   
    tile_array: the array of tiles of the checker board
    active_checker: a pointer to the checker whose moves/jumps to clear
    red_checkers: the array of red checkers
    blue_checkers: the array of blue checkers
    active_tile: the tile occupied by the active checker
    destination_tile: the tile occupied by our tertiary selection
   */
  
  // the checker of the jump, if applicable
  uint8_t rm_tile = (active_tile + destination_tile) / 2;

  // draw over the three tiles in a given diagonal from a checker
  draw_tile(tile_array, red_checkers, blue_checkers, active_tile);
  draw_tile(tile_array, red_checkers, blue_checkers, destination_tile);
  // not always jumping, but redrawing an additional tile causes no issues
  draw_tile(tile_array, red_checkers, blue_checkers, rm_tile);

  // draw over all the move and jump tiles of the given checker
  for (uint8_t i = 0; i < POSSIBLE_MOVES; i++){
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

void win_screen(int8_t turn){
  // draws a win screen for whosever turn it is, and initiates a new game
  delay(500);
  if(turn == TURN_BLUE){
    tft.fillRect(BORDER_WIDTH, BORDER_WIDTH, SCREEN_WIDTH - (2*BORDER_WIDTH)
		 ,SCREEN_WIDTH - (2*BORDER_WIDTH), ST7735_BLACK);
    tft.setCursor(WINSTART_X, WINSTART_Y);
    tft.setTextColor(ST7735_BLUE);
    tft.setTextSize(4);
    tft.print("BLUE");
    tft.setCursor(WINSTART_X, WINSTART_Y + WIN_INCREMENT);
    tft.print("TEAM");
    tft.setCursor(WINSTART_X, WINSTART_Y + (2*WIN_INCREMENT));
    tft.print("WINS");
  }
  else if(turn == TURN_RED){
    tft.fillRect(BORDER_WIDTH, BORDER_WIDTH, SCREEN_WIDTH - (2*BORDER_WIDTH)
		 ,SCREEN_WIDTH - (2*BORDER_WIDTH), ST7735_BLACK);
    tft.setCursor(WINSTART_X + RED_OFFSET, WINSTART_Y);
    tft.setTextColor(ST7735_RED);
    tft.setTextSize(4);
    tft.print("RED");
    tft.setCursor(WINSTART_X, WINSTART_Y + WIN_INCREMENT);
    tft.print("TEAM");
    tft.setCursor(WINSTART_X, WINSTART_Y + (2*WIN_INCREMENT));
    tft.print("WINS");
  }
  play_victory_music();
  delay(10000);
  game_state = SETUP_MODE;
}
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
    Serial.println("You called the populate graveyard function improperly");
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
  
  if (num_dead == CHECKERS_PER_SIDE) {
    win_screen(turn);
  }
}


void change_turn()
{
  /*
    this function is called to indicate on the lcd display whose turn it is,
    and does so by coloring the border their respective color; also sets the 
    active checker array and player dead to the new respective array and dead
    of the now-active player.

    due to difficulties with pointers, this procedure unfortunately takes no
    arguments and instead alters the global values, which include:

    player_turn, red_checkers, red_dead, blue_checkers, blue_dead

    
   */

  // since player turns are given by -1 and 1, multiplying by -1 changes to
  // the opposite turn
  player_turn = (-1) * player_turn; // change player turn to opposite turn

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


//Sub0.302: move and jump computing

uint8_t* compute_checker_jumps(Tile* tile_array, Checker* checker,
			       int8_t opp_color, uint8_t djump)
{
  /*
    this function checks the potential jumps of a given checker by checking
    the two respective diagonal tiles it may have an opponent in (all four
    if the checker is kinged) as well as the next diagonal space over in each
    direction, where:
    
    tile_array: the address of the array containing the checker board tiles
    checker: the checker in which we are interested
    opp_color: the color of the opponents checkers
    djump: whether we are checking for double jumps
   */

  // since each player has their colors given by -1 and 1, these also serve as
  // directions for checking jumps
  int8_t dir = opp_color;

  // directions are from the perspective of the checker:
  // forward-left
  if (tile_array[coord_to_tile(checker->x_tile -1, 
			    checker->y_tile - dir)].has_checker == opp_color){
    if (tile_array[coord_to_tile(checker->x_tile - 2,
				  checker->y_tile - 2*dir)].has_checker == 0){
      checker->jumps[0] = coord_to_tile(checker->x_tile - 2, 
				     checker->y_tile - 2*dir);
      checker->must_jump = 1;
    }
  }

  // forward-right
  if (tile_array[coord_to_tile(checker->x_tile + 1 , 
			    checker->y_tile - dir)].has_checker == opp_color){
    if (tile_array[coord_to_tile(checker->x_tile + 2,
				 checker->y_tile - 2*dir)].has_checker == 0) {
      checker->jumps[1] = coord_to_tile(checker->x_tile + 2, 
				     checker->y_tile - 2*dir);
      checker->must_jump = 1;
    }
  }
  if(checker->is_kinged || djump){

    // backward-left
    if (tile_array[coord_to_tile(checker->x_tile - 1, 
			    checker->y_tile + dir)].has_checker == opp_color){
      if (tile_array[coord_to_tile(checker->x_tile - 2,
				  checker->y_tile + 2*dir)].has_checker == 0){
	checker->jumps[2] = coord_to_tile(checker->x_tile - 2, 
				       checker->y_tile + 2*dir);
	checker->must_jump = 1;
      }
    }
    // backward-right
    if (tile_array[coord_to_tile(checker->x_tile + 1, 
			    checker->y_tile + dir)].has_checker == opp_color){
      if (tile_array[coord_to_tile(checker->x_tile + 2,
				  checker->y_tile + 2*dir)].has_checker == 0){
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
  /*
    this function checks the potential moves of a given checker by checking
    the two respective diagonal tiles it may have an open move in (all four
    if the checker is kinged), where:
    
    tile_array: the address of the array containing the checker board tiles
    checker: the checker in which we are interested
    opp_color: the color of the opponent's checkers

  */

  // since each player has their colors given by -1 and 1, these also serve as
  // directions for checking moves
  int8_t dir = opp_color;

  // directions are from the perspective of the checker:
  // forward-left
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
    // backward-left
    if (tile_array[coord_to_tile(checker->x_tile - 1, 
			    checker->y_tile + dir)].has_checker == 0) {
      checker->moves[2] = coord_to_tile(checker->x_tile - 1, checker->y_tile +
					dir);
    }
    // backward-right
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
  /*
    computes the moves and jumps of an array of checker structs, where:

    tile_array: the array of tiles of the checker board
    checkers: the array of checker structs we are computing for
    opp_color: the color of the opponents checkers

   */

  uint8_t no_fjumps = 1;  // no force jumps until proven otherwise:

  for (uint8_t i = 0; i < CHECKERS_PER_SIDE; i++){

    if (checkers[i].in_play){ // don't compute for those already dead

      compute_checker_moves(tile_array, &checkers[i], opp_color);
      compute_checker_jumps(tile_array, &checkers[i], opp_color, 0);

      if (checkers[i].must_jump){ // check whether there is a jump
	no_fjumps = 0;
      }
    }
  }
  return no_fjumps; // returns uint8_t
}


// Sub0.303: highlighting

void highlight_tile(uint8_t tile_num, int16_t mode)
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
  else {color = mode; }

  uint8_t* x_y = tile_to_coord(tile_num);
  uint8_t col = BORDER_WIDTH + (TILE_SIZE * x_y[0]);
  uint8_t row = BORDER_WIDTH + (TILE_SIZE * x_y[1]);
  
  tft.drawRect(col, row, TILE_SIZE, TILE_SIZE, color);
}

void highlight_moves(Checker* active_checker)
{
  // highlights the moves of the checker given, unless equal to the void tile
  for (uint8_t i = 0; i < POSSIBLE_MOVES; i++){
    if (active_checker->moves[i] != VOID_TILE){
      highlight_tile(active_checker->moves[i], MOVE_HIGHLIGHT);
    }
  }
}

void highlight_jumps(Checker* active_checker)
{
  // highlights the jumps of the checker given, unless equal to the void tile
  for (uint8_t i = 0; i < POSSIBLE_MOVES; i++){
    if (active_checker->jumps[i] != VOID_TILE){
      highlight_tile(active_checker->jumps[i], JUMP_HIGHLIGHT);
    }
  }
}

// Sub0.304: move & jump verification

uint8_t selection_matches_move(uint8_t selection, Checker* active_checker,
			       uint8_t check_moves)
{
  /*
    determines whether the given checker can move to the tile selected, where:
    
    selection: the tile selected
    active_checker: the checker whose moves we are comparing to the selection
    check_moves: whether to check the checkers moves or jumps to selection
   */

  // no matches until proven otherwise
  uint8_t matches = 0;

  for (uint8_t i = 0; i < POSSIBLE_MOVES; i++){
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
  // determines whether the given checker has an available move
  uint8_t matches = 0;

  for (uint8_t i = 0; i < POSSIBLE_MOVES; i++){

    if (active_checker->moves[i] != VOID_TILE){
      matches = 1;
    }
  }
  return matches;
}	

uint8_t check_must_jump(Checker* active_checker)
{
  // determines whether the given checker has an available jump
  uint8_t matches = 0;
  for (uint8_t i = 0; i < POSSIBLE_MOVES; i++){
    if (active_checker->jumps[i] != VOID_TILE){
      matches = 1;
    }
  }
  return matches;
}		    

uint8_t player_has_move(Checker* player_checkers) {
  // determines whether the given player has a checker with a move
  uint8_t player_has_move = 1;
  for (uint8_t i = 0; i < CHECKERS_PER_SIDE; i++) {
    if (check_can_move(&player_checkers[i])) {
      player_has_move = 0;
    }
  }
  return player_has_move;
}


// Sub0.305: checker jumping & movement

void move_checker(Tile* tile_array, Checker* active_checker, 
		  uint8_t active_tile, uint8_t destination_tile)
{
  /*
    move a checker from its current position to new position at destination
    tile; we don't have to worry about whether this move is valid, since this
    is already checked before calling this procedure, where:

    tile_array: the array of tiles of our checker board
    active_checker: the checker we are moving
    active_tile: the tile of the checker being moved
    destination_tile: the tile to which the checker is moving
    
   */

  tile_array[destination_tile].has_checker = 
    tile_array[active_tile].has_checker;
  tile_array[destination_tile].checker_num = 
    tile_array[active_tile].checker_num;

  tile_array[active_tile].has_checker = 0;
  tile_array[active_tile].checker_num = 13;

  uint8_t* x_y = tile_to_coord(destination_tile);
  // king the checker if it moved into the 0th or 7th row

  active_checker->x_tile = x_y[0];
  active_checker->y_tile = x_y[1];
  if(x_y[1] == 0 || x_y[1] == 7){
    active_checker->is_kinged = 1;
  }
  play_move_sound();
}


uint8_t jump_checker(Tile* tile_array, Checker* active_checker, 
		  uint8_t active_tile, uint8_t destination_tile)
{
  /* 
     jump a checker to the destination tile and return the index of the tile
     just jumped; we don't need to worry about whether this jump is valid, 
     since this is checked in another procedure, where:

     tile_array: the array of tiles of our checker board
     active_checker: the checker that is jumping
     active_tile: the tile from which the checker is jumping
     destination_tile: the tile to which to checker is jumping
   */
  tile_array[destination_tile].has_checker = 
    tile_array[active_tile].has_checker;
  tile_array[destination_tile].checker_num = 
    tile_array[active_tile].checker_num;

  tile_array[active_tile].has_checker = 0;
  tile_array[active_tile].checker_num = 13;
  
  // the tile jumped is the one between the active and destination tiles:
  uint8_t rm_tile =  (active_tile + destination_tile) / 2; // avg of the two

  uint8_t* x_y = tile_to_coord(destination_tile);
  active_checker->x_tile = x_y[0];
  active_checker->y_tile = x_y[1];


  if(x_y[1] == 0 || x_y[1] == 7){
    active_checker->is_kinged = 1;
  }

  play_jump_sound();

  return rm_tile;
}


// Sub0.307: joystick tile manipulation

uint8_t modify_tile_select(int joy_x, int joy_y, uint8_t* tile_highlighted)
{
  /* 
     modifies the tile being highlighted, where:
     joy_x, joy_y: the horizontal/vertical reading of the joystick
     tile_highlighted: pointer to the tile currently highlighted
  */

  // modifies tile selection without converting to x,y coords (was easier than
  // converting back and forth)

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


// Sub0.308: nicer boolean functions

uint8_t player_piece_on_tile(Tile* tile_array, uint8_t tile_highlighted, 
			     int8_t current_turn)
{
  // determines whether the current player's piece is on the tile; a bit more
  // instructive than the below statement returned
  return (tile_array[tile_highlighted].has_checker == current_turn);
}

// Sub0.309: debounce reset
void bouncer_reset()
{
  // debounce reset procedure, called by our interrupt operating off of the
  // system clock (in our case, of the arduino)

  if(bouncer < 3){
    bouncer++;
  }
}

// Sub0.310: sounds & music

void play_jump_sound(){
  // play the movement sound
  for(int i = 500; i < 1001; i = i+500){
    tone(SPEAKER_PIN, i, 100);
    delay(100);
  }
}

void play_move_sound(){
  tone(SPEAKER_PIN, NOTE_D4, DUR_MOVE);
}
// Final Fantasy Victory Fanfare
// Copyright © 2012 SQUARE ENIX CO., LTD
// Used less than ten notes, which is probably okay?
void play_victory_music(){
  tone(SPEAKER_PIN, NOTE_EB5, TRIPLET_EIGHTH);
  delay(TRIPLET_EIGHTH);
  tone(SPEAKER_PIN, NOTE_EB5, TRIPLET_EIGHTH);
  delay(TRIPLET_EIGHTH);
  tone(SPEAKER_PIN, NOTE_EB5, TRIPLET_EIGHTH);
  delay(TRIPLET_EIGHTH);
  tone(SPEAKER_PIN, NOTE_EB5, QUARTER_NOTE);
  delay(QUARTER_NOTE);
  tone(SPEAKER_PIN, NOTE_B4, QUARTER_NOTE);
  delay(QUARTER_NOTE);
  tone(SPEAKER_PIN, NOTE_DB5, QUARTER_NOTE);
  delay(QUARTER_NOTE);
  tone(SPEAKER_PIN, NOTE_EB5, TRIPLET_EIGHTH);
  delay(TRIPLET_EIGHTH);
  tone(SPEAKER_PIN, REST, TRIPLET_EIGHTH);
  delay(TRIPLET_EIGHTH);
  tone(SPEAKER_PIN, NOTE_DB5, TRIPLET_EIGHTH);
  delay(TRIPLET_EIGHTH);
  tone(SPEAKER_PIN, NOTE_EB5, DOTTED_HALF);
  delay(DOTTED_HALF);
}

// Sub0.310: debug procedures
// these will not operate without the debug button in place!!!!
void print_all_data(Tile* tile_array, Checker* red_checkers, 
		    Checker* blue_checkers, Checker* player_checkers){
  // massive debug procedure that prints all of the checker data to the serial
  // monitor (there were bad bugs, in case you were wondering)
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
    for(int j = 0; j < POSSIBLE_MOVES; j++){
      Serial.print(red_checkers[i].jumps[j]); Serial.print(", ");
      if(red_checkers[i].jumps[j] < 10){ Serial.print(" ");}
    }
    for(int j = 0; j < POSSIBLE_MOVES; j++){
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
    for(int j = 0; j < POSSIBLE_MOVES; j++){
      Serial.print(blue_checkers[i].jumps[j]); Serial.print(", ");
      if(blue_checkers[i].jumps[j] < 10){ Serial.print(" ");}
    }
    for(int j = 0; j < POSSIBLE_MOVES; j++){
      Serial.print(blue_checkers[i].moves[j]); Serial.print(", ");
      if(blue_checkers[i].moves[j] < 10){ Serial.print(" ");} 
    }
    Serial.println();
  }
  Serial.println();
  Serial.println();
}

void print_board_data(Tile* tile_array){
  // Prints the board to serial-mon for debugging purposes
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
    for (int j = 0; j < POSSIBLE_MOVES; j++) {
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
    for (int j = 0; j < POSSIBLE_MOVES; j++) {
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
    for (int j = 0; j < POSSIBLE_MOVES; j++) {
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
    for (int j = 0; j < POSSIBLE_MOVES; j++) {
      Serial.print(blue_checkers[i].jumps[j]); Serial.print(", ");
    }
    Serial.print("  location: ("); Serial.print(blue_checkers[i].x_tile);
    Serial.print(", "); Serial.print(blue_checkers[i].y_tile); 
    Serial.print(" ) ");
    Serial.println();
    Serial.println();
  }
}



//****************************************************************************
//                          Sec0.4: Setup Procedure     
//****************************************************************************

void setup()
{
  // Sub0.400: serial monitor & sd card preliminaries
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

  // Sub0.401 drawing the checker board


  // Sub0.402: set up pins
  // joystick
  pinMode(JOYSTICK_BUTTON, INPUT);     // read joybutton inputs
  digitalWrite(JOYSTICK_BUTTON, HIGH); // button presses set voltage to LOW!!

  pinMode(SPEAKER_PIN, OUTPUT);

  // debug button (optional)
  pinMode(DEBUG_BUTTON, INPUT);
  digitalWrite(DEBUG_BUTTON, HIGH);

  // Sub0.403: calibrate the joystick
  joy_x = analogRead(JOYSTICK_HORIZ); // the joystick must be in NEUTRAL 
  joy_y = analogRead(JOYSTICK_VERT);  // POSITION AT THIS STAGE!
  /* determine the minimum x and y remap values, so that we map the default
     reading of the x and y position of the joystick to exactly 0; formula
     was mathematically determined from the Arduino map function's source code
     casting here is necessary to avoid overflow when multiplying by 1000. */
  joy_min_x = (((int32_t) joy_x) * JOY_REMAP_MAX) / (joy_x - VOLT_MAX);
  joy_min_y = (((int32_t) joy_y) * JOY_REMAP_MAX) / (joy_y - VOLT_MAX);

  // Sub0.404: initialize time-based interrupt
  Timer3.initialize();
  Timer3.attachInterrupt(bouncer_reset, BOUNCE_PERIOD);
}


//****************************************************************************
//                        Sec0.5: Arduino Loop Procedure
//****************************************************************************

void loop()
{

  // Sub0.500: joystick reading & calibration

  // read joystick input regardless of mode
  joy_x = analogRead(JOYSTICK_HORIZ);
  joy_y = analogRead(JOYSTICK_VERT);
  // remap joy_x and joy_y to values in the range ~-1000 - 1000
  joy_x = map((joy_x), VOLT_MIN, VOLT_MAX, joy_min_x, JOY_REMAP_MAX);
  joy_y = map((joy_y), VOLT_MIN, VOLT_MAX, joy_min_y, JOY_REMAP_MAX);

  if (game_state == SETUP_MODE){
    // Sub0.501: game setup

    tft.fillScreen(0); // fill to black  
    lcd_image_draw(&cb_img, &tft, 0, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Fill the red checker array with blank checkers
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

    // Fill the blue checker array with blank checkers
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
    
    red_dead = 0;
    blue_dead = 0;
    player_turn = TURN_RED;
    turn_change = 1;
    tile_highlighted = DEFAULT_TILE;
    game_state = PLAY_MODE;
    tile_array[0].has_checker = 42;

  }

  else if (game_state == PLAY_MODE){

    // Sub0.502: changing turns

    if (turn_change){
      change_turn();
      // highlight the last tile highlighted with the new player's turn
      highlight_tile(tile_highlighted, player_turn);

      for (int i = 0; i < CHECKERS_PER_SIDE; i++){
	// assume all previous moves are now invalid, and clear new checkers
	player_checkers[i].must_jump = 0;
	for (int j = 0; j < POSSIBLE_MOVES; j++){
	  player_checkers[i].moves[j] = VOID_TILE;
	  player_checkers[i].jumps[j] = VOID_TILE;
	}
      }
      // recompute moves and jumps for all checkers in current player's chkers
      no_fjumps = compute_moves(tile_array, player_checkers, 
				(-1) * player_turn);
      no_moves = player_has_move(player_checkers);

      if (no_moves) {
	win_screen((-1) * player_turn);
      }
      turn_change = 0;

    }

    // Sub0.503: reading joystick movement

    // set the highlight movement delay time
    if (abs(joy_x) > 900 || abs(joy_y) > 900){ joy_delay_time = 50; }
    else { joy_delay_time = 220; }
    
    joy_x = joy_x / 400; // in range [-1, 1]
    joy_y = joy_y / 400;

    if ((joy_x != 0 || joy_y != 0) && !cursor_mode) { // moved, not selecting
      // modify the primary tile highlight
      draw_tile(tile_array, red_checkers, blue_checkers, tile_highlighted);
      modify_tile_select(joy_x, joy_y, &tile_highlighted);

      // redraw certain tiles
      highlight_tile(tile_highlighted, player_turn);
      delay(joy_delay_time);
    }

    else if ((joy_x !=0 || joy_y != 0) && cursor_mode){ // moved, selecting
      // modify the secondary tile highlight
      draw_tile(tile_array, red_checkers, blue_checkers, subtile_highlighted);
      modify_tile_select(joy_x, joy_y, &subtile_highlighted);

      // draw over old tiles, with precedence: moves/jumps>subtile>tile
      if (no_fjumps){ highlight_moves(active_checker); }
      else { highlight_jumps(active_checker); }
      highlight_tile(subtile_highlighted, player_turn);
      highlight_tile(tile_highlighted, TILE_HIGHLIGHT);
      delay(joy_delay_time);

    }

    // Sub0.504: reading button presses

    if (digitalRead(JOYSTICK_BUTTON) == LOW && bouncer > 1){
      bouncer = 0; // debounce
      if (cursor_mode == TILE_MOVEMENT) {

	// Sub0.505: checker selection

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

	  // Sub0.506: move selection

	  if (selection_matches_move(subtile_highlighted, active_checker, 
				     no_fjumps)) {
	    // move checker
	    move_checker(tile_array, active_checker, 
			 tile_highlighted, subtile_highlighted);
	    clear_draw(tile_array, active_checker, red_checkers, 
		       blue_checkers, tile_highlighted, subtile_highlighted);
	    turn_change = 1;
	    highlight_tile(tile_highlighted, player_turn);
	  }
	  else if(!checker_locked){
	    clear_draw(tile_array, active_checker, red_checkers, 
	               blue_checkers, tile_highlighted, subtile_highlighted);
	    highlight_tile(tile_highlighted, player_turn);

	  }
	  cursor_mode = TILE_MOVEMENT;
	}
	else { // there are fjumps

	  // Sub0.507: jump selection

	  if(selection_matches_move(subtile_highlighted, active_checker, 
				    no_fjumps)) {
	    // jump checker
	    rm_tile = jump_checker(tile_array, active_checker, 
				   tile_highlighted, subtile_highlighted);


	    // get rid of the checker we just jumped; messy, so seperated
	    // ***************************************************************
	    if ((player_turn) == TURN_RED) {
	      blue_checkers[tile_array[rm_tile].checker_num].in_play = 0;
	      for (uint8_t i = 0; i < POSSIBLE_MOVES; i++){
		blue_checkers[tile_array[rm_tile].checker_num].moves[i] = 
		  VOID_TILE;
		blue_checkers[(tile_array[rm_tile].checker_num)].jumps[i] = 
		  VOID_TILE;
	      }
	    }
	    else {
	      red_checkers[tile_array[rm_tile].checker_num].in_play = 0;
	      for (uint8_t i = 0; i < POSSIBLE_MOVES; i++){
		red_checkers[tile_array[rm_tile].checker_num].moves[i] = 
		  VOID_TILE;
		red_checkers[tile_array[rm_tile].checker_num].jumps[i] = 
		  VOID_TILE;
	      }
	    }
	    
	    tile_array[rm_tile].has_checker = 0;
	    tile_array[rm_tile].checker_num = 13;

	    clear_draw(tile_array, active_checker, red_checkers, 
		       blue_checkers, tile_highlighted, subtile_highlighted);

	    // nullify the jumping checkers moves and jumps, for now
	    for (uint8_t i = 0; i < POSSIBLE_MOVES; i++){
	      active_checker->moves[i] = VOID_TILE;
	      active_checker->jumps[i] = VOID_TILE;
	    }

	    //****************************************************************

	    (*player_dead) = (*player_dead) + 1;
	    populate_graveyard(*player_dead, player_turn);

	    checker_locked = 0;
	    // now handle double jumping:
	    if (active_checker->y_tile != 0 && active_checker->y_tile != 7) {
	      // recompute the jumping checker's jumps
	      compute_checker_jumps(tile_array, active_checker,
				    (-1) * player_turn, 1);
	      if (check_must_jump(active_checker)) { // if the piece can jump
		tile_highlighted = subtile_highlighted;
		highlight_tile(subtile_highlighted, TILE_HIGHLIGHT);
		highlight_jumps(active_checker);
		checker_locked = 1;
	      }
	      else { // else change turn
		turn_change = 1;
		cursor_mode = TILE_MOVEMENT;
		highlight_tile(tile_highlighted, player_turn);
	      }
	    }
	    else { // else change turn
	      turn_change = 1;
	      cursor_mode = TILE_MOVEMENT;
	      highlight_tile(tile_highlighted, player_turn);
	    }

	  }
	  else if(!checker_locked) {
	    clear_draw(tile_array, active_checker, red_checkers, 
		       blue_checkers, tile_highlighted, subtile_highlighted);
	    cursor_mode = TILE_MOVEMENT;
	    highlight_tile(tile_highlighted, player_turn);
	    
	  }
	}
      }
    } // end button press if

    // Sub0.508: debug prompt
    else if (digitalRead(DEBUG_BUTTON) == LOW){print_all_data(tile_array,
			    red_checkers, blue_checkers, player_checkers);}
  }
}

