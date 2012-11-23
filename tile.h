#ifndef _TILE_H
#define _TILE_H

/* 
   Struct for a tile of a checker board, where:

   has_checker: tile has a checker, 0 for no checker, r for red, b for black
   checker_num: the index of the contained checker in its array, 0 to 11

   for a total of 2 bytes per tile x 64 = 128 bytes
 */

typedef struct {
  char has_checker;
  uint8_t checker_num;
} Tile;

#endif
