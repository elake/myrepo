#ifndef _CHECKER_H
#define _CHECKER_H

/*
  Struct for a checker piece, where:

  x_tile, y_tile: the horiz/vert tile the checker is on, 0 to 7 for each
  king_status:    whether the checker piece is kinged, 0 or 1
  in_play:        whether the checker is still on the board, 0 or 1
  must_jump:      whether the checker has to jump in the next turn

  for a total of 5 bytes per checker x 24 = 120 bytes
 */
typedef struct {
  uint8_t x_tile;
  uint8_t y_tile;
  uint8_t is_kinged;
  uint8_t in_play;
  uint8_t must_jump;
  uint8_t jumps[4];
} Checker;

#endif
