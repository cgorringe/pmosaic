/*-----------------------------------------------------------------------------
  mosaic.c
  Copyright (c) 2010 Carl Gorringe - carl.gorringe.org
  6/4/2010
 
  Description:

  Usage:
    mosaic tile_bin.db < input.csv > output.csv

  Dependencies:
  * apt-get install libjudy-dev  (from universe)
    man judy  (for usage)

  * compile source with:
      gcc [flags] sourcefiles -lJudy


  See test_runs.txt for runtimes for this program.
  -----------------------------------------------------------------------------
  (Almost DONE, works!)

  TODO:
  [X] mosaics where dups < numTiles (e.g. those w/ unique tiles)
  [+] vertically flipped tiles
    [ ] currently duplicates tile (so 2 of same tile when dups = 1) may need to fix.
 
  -----------------------------------------------------------------------------
*/

// Libraries
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>  // already in stdlib?
#include <stdint.h>  // defines int32_t, int64_t, uint8_t
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include <Judy.h>   // requires libjudy installed, and compile flag -lJudy
#include "mosaic.h"

/*
  // potentially faster absolute value?
  inline int abs(int x) { return (x < 0) ? -x : x; }
  // or just use the <stdlib.h> abs() function, which should produce fastest code.

  OR Macro version:
  #define abs(x)  (((x) >= 0) ? (x) : -(x))

  // another version that avoids branching (not tested)
  inline int abs(int n) {
    int const mask = n >> (sizeof(int) * 8 - 1);
    return ((n + mask) ^ mask);
  }
*/

/*-----------------------------------------------------------------------------
CSV Input:

  Xtiles, Ytiles, Xblocks, Yblocks, Flags, Wy, Wc, We, dups
  pos1, Ydelta1, matrix1...
  pos2, Ydelta2, matrix2...
  ...
  posN, YdeltaN, matrixN...

  Where:
    N <= Xtiles * Ytiles;  // N can be less because some tiles may be pre-filled
    Xtiles, Ytiles is number of tiles across and down in image.
    Xblocks, Yblocks = blocks per tile (default = 8,8 = 64 blocks)
    Flags =  0x00: original tiles
             0x01: LumFlag, adjust tile brightness.
             0x02: VFlipFlag, test both original and vertically flipped tiles.
    Wy = luma weight  as int (default = 1)
    Wc = color weight as int (default = 1)
    We = edge weight  as int (default = 0)
    pos = (Ypos * Xtiles) + Xpos; // where Xpos & Ypos start at 0.

  A matrix is formatted as:
    Y1, U1, V1, E1, Y2, U2, V2, E2, ... Yn, Un, Vn, En
    where n = 8*8 = 64 (or number of blocks per tile)
    Using the YUV colorspace, where Y is luma (brightness), 
    U & V are chrominance (color) components, E is edge intensity.
    Y is normalized. Add Ydelta to Y to get actual value.

CSV Output:

  Xtiles, Ytiles, Blocks, Flags, Wy, Wc, We, dups
  pos1, Ydelta1, imageID1
  pos2, Ydelta2, imageID2
  ...
  posN, YdeltaN, imageIDN

  -----------------------------------------------------------------------------
*/

/*
 Algorithm:
  Take lib image record:
    imageID (4-byte long int)
    Ydelta  (2-byte signed int)
    Reserved (2-byte int)
    matrixD (N*N*4 bytes) which contain average values {Y, U, V, E}
    (Total = 4 + 2 + 2 + 8*8*4 = 8 + 256 = 264 bytes per image)
*/

//-----------------------------------------------------------------------------
void die(char* errMsg)
{
  fprintf(stderr, "\nERROR: %s\n", errMsg);
  exit(1);
}

//-----------------------------------------------------------------------------

void initArrays(int numTiles, int numBlocks, TileRecord *tileImg, 
                TileScore **tileScores)
{
  int i, j;

  // allocate memory for arrays (doesn't work here, why??)
/*
  tileImg = (TileRecord *) calloc(numTiles, sizeof(TileRecord));
  if (tileImg == NULL) die("Could not init tileImg");
  tileScores = (TileScore **) malloc(numTiles * sizeof(TileScore *));
  if (tileScores == NULL) die("Could not init tileScores");

  for (i=0; i < numTiles; i++) {
    tileScores[i] = (TileScore *) malloc(numTiles * sizeof(TileScore));
    if (tileScores[i] == NULL) die("Could not init tileScores[i]");
  }
*/

  // init arrays
  for (i=0; i < numTiles; i++) {

    tileImg[i].imageID = i+1;
    // following is cleared already by calloc()
/*
    // clear master image tiles
    // (problably faster to use some calloc() function)
    tileImg[i].magic = 0;
    tileImg[i].imageID = i+1;
    tileImg[i].Ydelta = 0;
    for (j=0; j < numBlocks; j++) {
      tileImg[i].pixel[j].Y = 0;
      tileImg[i].pixel[j].U = 0;
      tileImg[i].pixel[j].V = 0;
      tileImg[i].pixel[j].E = 0;
    }
*/

    // init sorted tile score matrix.
    //for (j=0; j < numTiles; j++) {
    for (j=0; j <= i; j++) {
      tileScores[i][j].score = INT_MAX;
      tileScores[i][j].id = 0;
    }
  }  
}

//-----------------------------------------------------------------------------
// Test: Print score/id matrix (600x600)

void testPrintScores(int numTiles, TileScore **tileScores)
{
  int i,j;
  fprintf(stderr, "DEBUG: Outputting Score Matrix to stdout:\n");
  fflush(stderr);

  for (i=0; i < numTiles; i++) {
    printf("(%d) ", i);
    //for (j=0; j < numTiles; j++) {
    for (j=0; j <= i; j++) {
      printf("%d:%d ", tileScores[i][j].id, tileScores[i][j].score);
    }
    printf("\n");
  }

}


//-----------------------------------------------------------------------------
// Processing a single library image.
// Finds best match for each tile position for a single library image.

/*
  Input:
    numTiles <= Xtiles * Ytiles;  // numTiles can be less because some tiles may be pre-filled
    numBlocks = blocks per tile (default = 8*8 = 64)
    LumFlag = { 0 = original tiles, 1 = adjust tile brightness }
    Wy =  luma weight (default = 1)
    Wc = color weight (default = 1)
    We =  edge weight (default = 0)
    dups = number of times a tile may be duplicated in mosaic. 
          (e.g. 1 = all tiles unique; 600 = may use same tile in every position in a 30x20 mosaic.)
*/

void processLibImg(TileRecord *libImg, TileRecord *tileImg, 
                    TileScore **tileScores,
                    int numTiles, int numBlocks, int lumFlag, int Wy, int Wc, int We, int dups)
{
  int score, Ydelta;
  int i, j, k;
  int dupCount = dups;
  //int shiftFlag;
  //TileScore shiftScore = {0, 0};
  //TileScore tempScore  = {0, 0};

  // Loop thru all tiles in master image. (e.g. 20*30 = 600)
  k=0;  // k is last index in tileScores, and increments
  for (i=0; i < numTiles; i++) {
    score = 0;

    // Compare tile with library image.
    // This runs in O(n) where n = numBlocks * numTiles (e.g. 8*8 = 64 * 600 = 38,400 calculations per libImg).
    // idea: if const BLOCKS used instead of numBlocks, compiler could unroll j loop.
    // idea: moving if statements outside for i loop would eliminate 2*600 branches.

    if (We) {
      // include edge values in score
      if (lumFlag) {
        // use normalized Y values
        for (j=0; j < numBlocks; j++) {
          score +=  Wy *    abs( tileImg[i].pixel[j].Y - libImg->pixel[j].Y )
                +   Wc * (( abs( tileImg[i].pixel[j].U - libImg->pixel[j].U ) 
                          + abs( tileImg[i].pixel[j].V - libImg->pixel[j].V )) >> 1)
                +   We *    abs( tileImg[i].pixel[j].E - libImg->pixel[j].E );
        }
      }
      else {
        // use original Y values
        Ydelta = tileImg[i].Ydelta - libImg->Ydelta;
        for (j=0; j < numBlocks; j++) {
          score +=  Wy *    abs( tileImg[i].pixel[j].Y - libImg->pixel[j].Y + Ydelta )
                +   Wc * (( abs( tileImg[i].pixel[j].U - libImg->pixel[j].U ) 
                          + abs( tileImg[i].pixel[j].V - libImg->pixel[j].V )) >> 1)
                +   We *    abs( tileImg[i].pixel[j].E - libImg->pixel[j].E );
        }
      }
    }
    else {
      // don't include edge values in score
      if (lumFlag) {
        // use normalized Y values
        for (j=0; j < numBlocks; j++) {
          score +=  Wy *    abs( tileImg[i].pixel[j].Y - libImg->pixel[j].Y )
                +   Wc * (( abs( tileImg[i].pixel[j].U - libImg->pixel[j].U ) 
                          + abs( tileImg[i].pixel[j].V - libImg->pixel[j].V )) >> 1);
        }
      }
      else {
        // use original Y values
        Ydelta = tileImg[i].Ydelta - libImg->Ydelta;
        for (j=0; j < numBlocks; j++) {
          score +=  Wy *    abs( tileImg[i].pixel[j].Y - libImg->pixel[j].Y + Ydelta )
                +   Wc * (( abs( tileImg[i].pixel[j].U - libImg->pixel[j].U ) 
                          + abs( tileImg[i].pixel[j].V - libImg->pixel[j].V )) >> 1);
        }
      }
    }  // end if (Wc > 0)

    // Insert score into tileScores[][] using insert sort. (lowest to highest score)
    // This previously ran in O(n) where n = numTiles (e.g. 20*30 = 600 * 600 < 360,000 array accesses per libImg).
    // TODO: Figure a way to optimize this!  Is there a better algorithm?
    // [x] idea #1: don't process entire square, just half triangle.
    // [x] idea #2: reduce j loop based on 'dups'.
    // [ ] scrap this idea: replace linear search with bsearch. (not needed)
    // [x] idea #3: skip linear search by shifting end first.

// *** Replacing this ***
/*
    // only insertsort if score is < highest value in array
    //if ( score < tileScores[i][numTiles - 1].score ) {
    //if ( score < tileScores[i][i].score ) {  // idea #1
    if ( score < tileScores[i][k].score ) {  // idea #2
      shiftFlag = 0;
      //for (j=0; j < numTiles; j++) {
      //for (j=0; j <= i; j++) {      // idea #1
      for (j=0; j <= k; j++) {      // idea #2
        if (shiftFlag) {
          // shift values in arrays down by 1 position
          tempScore = shiftScore;
          shiftScore = tileScores[i][j];
          tileScores[i][j] = tempScore;
        }
        else if ( score < tileScores[i][j].score ) {
          // gets here only once until next i
          shiftFlag = 1;
          shiftScore = tileScores[i][j];
          tileScores[i][j].score = score;
          tileScores[i][j].id = libImg->imageID;
        }
      } // next j
    } // end if
*/
    // *** New improved insertion sort *** idea#3

    // only insertsort if score is < highest value in array
    if ( score < tileScores[i][k].score ) {  // idea #2
      j = k;
      while ((j != 0) && (score < tileScores[i][j-1].score)) {
        // shift score down by 1 position
        tileScores[i][j] = tileScores[i][j-1];
        j--;
      }
      // store new score
      tileScores[i][j].score = score;
      tileScores[i][j].id = libImg->imageID;
    } // end if

    if ((--dupCount) == 0) { dupCount = dups; k++; }  // idea #2
  } // next i

} // end function

//-----------------------------------------------------------------------------
// Read Master Image Tiles in CSV format from given file stream.
// Returns: TileRecord tileImg[]
// (done, not tested)

/*
CSV Input:  (where n = numBlocks = 64; N = numTiles <= 600)
  Xtiles, Ytiles, numBlocks, LumFlag, Wy, Wc, We, dups
  pos1, Ydelta1, Y1, U1, V1, E1, Y2, U2, V2, E2, ... Yn, Un, Vn, En
  pos2, Ydelta2, ...
  ...
  posN, YdeltaN, ...
*/
// TODO: [ ] MUST Handle EOF case before numTiles reached!

void readTilesFromCSV(FILE *in , int numTiles, int numBlocks, TileRecord *tileImg )
{
  int i, j, temp, t1, t2, t3, t4;

  for (i=0; i < numTiles; i++) {
    // fprintf(stderr, "(i=%d) ", i);  // ** TEST **
    tileImg[i].magic = 0;
    temp = fscanf(in, " %d , %d ", &t1, &t2);
    if (temp != 2) die("CSV input missing values or incorrectly formatted! (first 2)");
    tileImg[i].imageID = t1;
    tileImg[i].Ydelta  = t2;

    for (j=0; j < numBlocks; j++) {
      // fprintf(stderr, "%d ", j);  // ** TEST **
      temp = fscanf(in, " , %d , %d , %d , %d ", &t1, &t2, &t3, &t4);
      if (temp != 4) die("CSV input missing values or incorrectly formatted!");
      tileImg[i].pixel[j].Y = t1;
      tileImg[i].pixel[j].U = t2;
      tileImg[i].pixel[j].V = t3;
      tileImg[i].pixel[j].E = t4;
    }
    // fprintf(stderr, "\n");  // ** TEST **

  } // next i

}

//-----------------------------------------------------------------------------
// Write mosaic as CSV
// (WORKS!)
/*
CSV Output:

  Xtiles, Ytiles, Blocks, Flags, Wy, Wc, We, dups
  pos1, Ydelta1, imageID1
  pos2, Ydelta2, imageID2
  ...
  posN, YdeltaN, imageIDN
*/
// NOTE: Where to get the Ydelta values???

void writeTiles( FILE *outfile, int numTiles, int dups, 
                 TileScore **tileScores, TileRecord *tileImg )
{
  int i, j, id=0, pos=0, y=0;

  // init Judy array
  Pvoid_t idList = (Pvoid_t) NULL;  // JudyL array (Judy.h required)
  Word_t   index;     // array index
  Word_t   value;    // array element value
  Word_t *pvalue;   // pointer to array element value
  //int      Rc_int;   // return code  (is this used??)


  if (numTiles == dups) {
    // output all best matching tiles, including all duplicates
    for (i=0; i < numTiles; i++) {
      id = tileScores[i][0].id;
      pos = tileImg[i].imageID;
      y = tileImg[i].Ydelta;   // TODO: change this
      fprintf(outfile, "%d,%d,%d\n", pos, y, id);
    }
  }
  else {
    // output tiles upto dups duplicate tiles (DONE, TEST #1 OK)
    //fprintf(stderr, "  writeTiles()  numTiles:%d  dups:%d \n", numTiles, dups);  // ** TEST **

    for (i=0; i < numTiles; i++) {
      pos = tileImg[i].imageID;
      y = tileImg[i].Ydelta;   // TODO: change this

      j=0;
      while(j <= i) {
        id = tileScores[i][j].id;
        //index = (Word_t) id;
        index = (Word_t) abs(id);    // taking abs() removes vflip duplicate ** BUG! **
        // Part of FIX is to double the num of scores per tile, but still have to figure this out. (WRONG)
        // REAL FIX: place flip in score calculation, compare flipped w/ non-flipped score, then do 1 insert sort.
        // Must do this in processLibImg(), only call this once! (unlike the 2nd call done now)

        JLG(pvalue, idList, index);  // JudyLGet()
        if (pvalue != NULL) {
          if (*pvalue < dups) {
            *pvalue += 1;
            break;  // exit while loop
          }
          // else continue loop
        }
        else {
          // insert a new value into Judy array
          JLI(pvalue, idList, index);  // JudyLIns()
          if (pvalue == PJERR) { die("Judy malloc() error!"); }
          *pvalue = 1;   // store new value
          break;   // exit while loop
        }
        j++;
      }

      fprintf(outfile, "%d,%d,%d\n", pos, y, id);
    }

    JLFA(value, idList);  // JudyLFreeArray()
    //fprintf(stderr, "  writeTiles()  idList bytes freed:%d \n", (int)value);  // ** TEST **

  }  // end if

}
//

//-----------------------------------------------------------------------------
// Swaps left with right pixels in tile, creating a vertically-flipped tile.
// (DONE, NOT TESTED)

void flipTileVertically(TileRecord *libImg, int Xblocks, int Yblocks) {
  int x, y, w, halfX, yline=0;
  TilePixel pixel;
  halfX = Xblocks >> 1;  // same as floor(Xblocks / 2)
  w = Xblocks - 1;

  for (y=0; y < Yblocks; y++) {
    for (x=0; x < halfX; x++) {
      // swap left with right pixel
      pixel = libImg->pixel[yline + x];
      libImg->pixel[yline + x] = libImg->pixel[yline + w - x];
      libImg->pixel[yline + w - x] = pixel;
    }
    yline += Xblocks;
  }
  libImg->imageID *= -1;  // negate image id for flipped images
}

//-----------------------------------------------------------------------------
void usage()
{
  fprintf(stderr, "Usage: mosaic tile_bin.db < input.csv > output.csv \n");
  exit(1);
}

//-----------------------------------------------------------------------------

int main(int argc, char * argv[])
{
  // init variables
  int numTiles = 600, Xtiles = 20, Ytiles = 30;
  int numBlocks = 64, Xblocks = 8, Yblocks = 8;
  int flags = 0, lumFlag = 0, vflipFlag = 0, Wy = 1, Wc = 1, We = 0, dups = 600;
  int i, numLibTiles, dbSize;
  time_t timeBegin, timeEnd;
  clock_t clockBegin, clockEnd;
  double clockDiff;
  FILE *DB;
  TileScore **tileScores;
  TileRecord *tileImg;
  TileRecord libImg;

  // process command line
  if (argc != 2) usage();
  fprintf(stderr, "Running mosaic\n");  // ** DEBUG **

  //--- Read master image tiles in CSV format from STDIN ---
  fprintf(stderr, "Reading master image in CSV format from STDIN...\n");  // ** DEBUG **

  int temp = fscanf(stdin, " %d , %d , %d, %d , %d , %d , %d , %d , %d ", 
                &Xtiles, &Ytiles, &Xblocks, &Yblocks, &flags, &Wy, &Wc, &We, &dups);
  if (temp != 9) die("CSV first line missing values or incorrectly formatted!");
  numTiles = Xtiles * Ytiles;  // assume all tiles present in input  TODO: can't really assume this!
  numBlocks = Xblocks * Yblocks;

  // set flags
  lumFlag   = (flags & 0x01);         // 0x01
  vflipFlag = (flags & 0x02) >> 1;    // 0x02

  //--- Allocate memory for arrays (TODO: move this to initArrays() ) ---
  tileImg = (TileRecord *) calloc(numTiles, sizeof(TileRecord));  // must use calloc
  if (tileImg == NULL) die("Could not init tileImg");
  tileScores = (TileScore **) malloc(numTiles * sizeof(TileScore *));
  if (tileScores == NULL) die("Could not init tileScores");
  for (i=0; i < numTiles; i++) {
    // tileScores[i] = (TileScore *) malloc(numTiles * sizeof(TileScore));
    tileScores[i] = (TileScore *) malloc((i+1) * sizeof(TileScore));  // half square
    if (tileScores[i] == NULL) die("Could not init tileScores[i]");
  }
  initArrays(numTiles, numBlocks, tileImg, tileScores);
  // testPrintScores(numTiles, tileScores);  // ** DEBUG **
  readTilesFromCSV(stdin, numTiles, numBlocks, tileImg);

  //--- Read and process every tile in library database file ---
  fprintf(stderr, "Reading tile database file: %s \n", argv[1] );

  // get file size of tile database
  struct stat statBuf;
  stat(argv[1], &statBuf);
  dbSize = statBuf.st_size;
  numLibTiles = dbSize / sizeof(TileRecord);
  fprintf(stderr, "  size:%d  numTiles:%d\n", dbSize, numLibTiles);
  if (dbSize < sizeof(TileRecord)) die("Tile database doesn't exist or zero size!");

  // open tile database
  if ((DB = fopen(argv[1], "rb")) == NULL) die("Cannot open tile database file!");
  
  //fprintf(stderr, "Reading Tile Library...\n");
  fprintf(stderr, "Computing Mosaic...\n");
  fprintf(stderr, "  tiles:%d  blocks:%d  lum:%d  vflip:%d  Wy:%d  Wc:%d  We:%d  dups:%d \n", 
          numTiles, numBlocks, lumFlag, vflipFlag, Wy, Wc, We, dups);
 
  timeBegin = time(NULL);
  clockBegin = clock();
  //fprintf(stderr, " start: %s", ctime(&timeBegin));

  // read entire tile database into memory (not any faster, so not used)
  /*
  TileRecord *libDB;
  libDB = (TileRecord*) malloc(sizeof(TileRecord) * numLibTiles);
  if (libDB == NULL) die("Could not allocate memory for tile database.");
  if ( fread(libDB, sizeof(TileRecord), numLibTiles, DB) != numLibTiles ) die("Problem reading entire tile database!");
  */

  //--- loop through every image in tile database ---
  //fprintf(stderr, "Computing Mosaic...\n");
  for (i=0; i < numLibTiles; i++) {

    // output current tile record number followed by CR to keep cursor on same line
    // skip every 10
    if ((i % 10) == 0) {
      fprintf(stderr, "%d  (%.1f%%)\r", i, (100.0f * (i+1)/numLibTiles) );
    }

    // read a tile one at a time
    if ( fread(&libImg, sizeof(TileRecord), 1, DB) != 1 ) die("Problem reading tile record!");

    // process one tile at a time
    if (libImg.magic != TILE_MAGIC) die("Tile magic number invalid.");
    processLibImg(&libImg, tileImg, tileScores, numTiles, numBlocks, lumFlag, Wy, Wc, We, dups);

    if (vflipFlag) {   // ** DONE, NOT TESTED **
      // flip the lib tile vertically, then process again
      flipTileVertically(&libImg, Xblocks, Yblocks);
      processLibImg(&libImg, tileImg, tileScores, numTiles, numBlocks, lumFlag, Wy, Wc, We, dups);
    }

    // if processing entire DB in memory (not any faster)
    //if (libDB[i].magic != TILE_MAGIC) die("Tile magic number invalid.");
    //processLibImg(&libDB[i], tileImg, tileScores, numTiles, numBlocks, lumFlag, Wy, Wc, We, dups);
  }
  clockEnd = clock();
  timeEnd = time(NULL);
  clockDiff = ((double) (clockEnd - clockBegin)) / CLOCKS_PER_SEC;
  fprintf(stderr, "\n");
  //fprintf(stderr, "   end: %s", ctime(&timeEnd));
  fprintf(stderr, "Mosaic took %.2f secs. (%.0f secs)\n", clockDiff, difftime(timeEnd, timeBegin) );

  // free if entire DB in memory (not used)
  //free(libDB);
  //libDB = NULL;

  // close tile database
  if (fclose(DB) != 0) die("Cannot close tile database file!");

  //testPrintScores(numTiles, tileScores);  // ** DEBUG **

  //--- output Mosaic CSV ---
  fprintf(stderr, "Outputing Mosaic CSV...\n");
  printf("%d,%d,%d,%d,%d,%d,%d,%d,%d\n", Xtiles, Ytiles, Xblocks, Yblocks, flags, Wy, Wc, We, dups);
  writeTiles(stdout, numTiles, dups, tileScores, tileImg);

  // free memory
  free(tileImg); tileImg = NULL;
  for (i=0; i < numTiles; i++) {
    free(tileScores[i]); tileScores[i] = NULL;
  }
  free(tileScores); tileScores = NULL;


  fprintf(stderr, "Done mosaic\n\n");
  return 0;
}

// EOF
