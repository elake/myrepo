#ifndef _CHECKER_H
#define _CHECKER_H

/*
  Struct for a checker piece, where:

  color:          what color the piece is, 'r' for red, or 'b' for black
  x_tile, y_tile: the horiz/vert tile the checker is on, 0 to 7 for each
  king_status:    whether the checker piece is kinged, 0 or 1
  in_play:        whether the checker is still on the board, 0 or 1

  for a total of 5 bytes per checker x 24 = 120 bytes
 */
typedef struct {
  char color;
  uint8_t x_tile;
  uint8_t y_tile;
  uint8_t king_status;
  uint8_t in_play;
} Checker;

#endif
