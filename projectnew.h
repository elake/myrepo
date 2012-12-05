#ifndef _PROJECTNEW_H
#define _PROJECTNEW_H

/*
  Map (x,y) coordinate to corresponding tile in 64-tile array, where:

  x: the horizontal tile
  y: the vertical tile

*/
uint8_t coord_to_tile(uint8_t x, uint8_t y);


/* 
   maps a tile number to x,y coordinate, where:
     
   tile_num - the tile index of the tile array
*/  
uint8_t* tile_to_coord(uint8_t tile_num);


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
void draw_tile(Tile* tile_array, Checker* red_checkers, 
	       Checker* blue_checkers, uint8_t tile_index);



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
void clear_draw(Tile* tile_array, Checker* active_checker,
		Checker* red_checkers, Checker* blue_checkers, 
		uint8_t active_tile, uint8_t destination_tile);


// draws a win screen for whosever turn it is, and initiates a new game
void win_screen(int8_t turn);


/*
  this function is called when a piece from either player dies, and puts the
  corresponding death number into the graveyard at the bottom of the screen,
  where:

  num_dead: the number of checkers dead for either side
  turn: whose turn it was when the checker died, so that a checker from the
  opposite side is committed to the graveyard
    
  uses globals: cbg_image, tft

*/
void populate_graveyard(uint8_t num_dead, int8_t turn);


/*
  this function is called to indicate on the lcd display whose turn it is,
  and does so by coloring the border their respective color; also sets the 
  active checker array and player dead to the new respective array and dead
  of the now-active player.

  due to difficulties with pointers, this procedure unfortunately takes no
  arguments and instead alters the global values, which include:

  player_turn, red_checkers, red_dead, blue_checkers, blue_dead

    
*/
void change_turn();


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
uint8_t* compute_checker_jumps(Tile* tile_array, Checker* checker,
			       int8_t opp_color, uint8_t djump);


/*
  this function checks the potential moves of a given checker by checking
  the two respective diagonal tiles it may have an open move in (all four
  if the checker is kinged), where:
    
  tile_array: the address of the array containing the checker board tiles
  checker: the checker in which we are interested
  opp_color: the color of the opponent's checkers

*/
void compute_checker_moves(Tile* tile_array, Checker* checker, 
			   int8_t opp_color);



/*
  computes the moves and jumps of an array of checker structs, where:

  tile_array: the array of tiles of the checker board
  checkers: the array of checker structs we are computing for
  opp_color: the color of the opponents checkers

*/
uint8_t compute_moves(Tile* tile_array, Checker* checkers, 
		      int8_t opp_color);


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
void highlight_tile(uint8_t tile_num, int16_t mode);


// highlights the moves of the checker given, unless equal to the void tile
void highlight_moves(Checker* active_checker);


// highlights the jumps of the checker given, unless equal to the void tile
void highlight_jumps(Checker* active_checker);



/*
  determines whether the given checker can move to the tile selected, where:
    
  selection: the tile selected
  active_checker: the checker whose moves we are comparing to the selection
  check_moves: whether to check the checkers moves or jumps to selection
*/
uint8_t selection_matches_move(uint8_t selection, Checker* active_checker,
			       uint8_t check_moves);


// determines whether the given checker has an available move
uint8_t check_can_move(Checker* active_checker);


// determines whether the given checker has an available jump
uint8_t check_must_jump(Checker* active_checker);



// determines whether the given player has a checker with a move
uint8_t player_has_move(Checker* player_checkers);



/*
  move a checker from its current position to new position at destination
  tile; we don't have to worry about whether this move is valid, since this
  is already checked before calling this procedure, where:

  tile_array: the array of tiles of our checker board
  active_checker: the checker we are moving
  active_tile: the tile of the checker being moved
  destination_tile: the tile to which the checker is moving
    
*/
void move_checker(Tile* tile_array, Checker* active_checker, 
		  uint8_t active_tile, uint8_t destination_tile);



/* 
   jump a checker to the destination tile and return the index of the tile
   just jumped; we don't need to worry about whether this jump is valid, 
   since this is checked in another procedure, where:

   tile_array: the array of tiles of our checker board
   active_checker: the checker that is jumping
   active_tile: the tile from which the checker is jumping
   destination_tile: the tile to which to checker is jumping
*/
uint8_t jump_checker(Tile* tile_array, Checker* active_checker, 
		     uint8_t active_tile, uint8_t destination_tile);



/* 
   modifies the tile being highlighted, where:
   joy_x, joy_y: the horizontal/vertical reading of the joystick
   tile_highlighted: pointer to the tile currently highlighted
*/
uint8_t modify_tile_select(int joy_x, int joy_y, uint8_t* tile_highlighted);



// determines whether the current player's piece is on the tile; a bit more
// instructive than the statement returned
uint8_t player_piece_on_tile(Tile* tile_array, uint8_t tile_highlighted, 
			     int8_t current_turn);



// debounce reset procedure, called by our interrupt operating off of the
// system clock (in our case, of the arduino)
void bouncer_reset();



// various sounds
void play_move_sound();

void play_jump_sound();

void play_victory_music();



// debug procedures
void print_all_data(Tile* tile_array, Checker* red_checkers, 
		    Checker* blue_checkers, Checker* player_checkers);

void print_board_data(Tile* tile_array);


#endif
