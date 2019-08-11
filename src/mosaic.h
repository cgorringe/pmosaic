/*-----------------------------------------------------------------------------
  mosaic.h
  Copyright (c) 2010-2019 Carl Gorringe - carl.gorringe.org
  6/4/2010
  8/10/2019
 
  -----------------------------------------------------------------------------
*/

#ifndef MOSAIC_H
#define MOSAIC_H

// Constants
#define BLOCKS  8*8               // number of blocks in a tile
#define TILE_MAGIC  0x454C4954    // ASCII 'TILE' in reverse byte order

#pragma pack(push)  // push current alignment to stack
#pragma pack(1)     // set alignment to 1 byte boundary

// Structures
typedef struct {
  uint8_t Y;
  uint8_t U;
  uint8_t V;
  uint8_t E;
} TilePixel;

typedef struct {
  int32_t magic;             //     4 bytes  'TILE'   (TILE_MAGIC)
  int32_t imageID;           //     4 bytes  int32_t
  int16_t Ydelta;            //     2 bytes  int16_t  range:[-255,+255]
  int16_t xres;              //     2 bytes  int16_t  xres of original image (replaced)
  int16_t yres;              //     2 bytes  int16_t  yres of original image (new)
  TilePixel pixel[BLOCKS];   // + 256 bytes
} TileRecord;                // = 270 bytes total (was 268)

#pragma pack(pop)   // restore original alignment from stack

typedef struct {
  int score;
  int32_t id;   // signed, negative value means tile flipped vertically
} TileScore;

#endif
