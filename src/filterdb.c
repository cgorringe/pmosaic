/*-----------------------------------------------------------------------------
  filterdb.c
  Copyright (c) 2019 Carl Gorringe - carl.gorringe.org
  8/10/2019
 
  -----------------------------------------------------------------------------
*/

// Libraries
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#include <Judy.h>   // requires libjudy installed, and compile flag -lJudy
#include "mosaic.h"

//-----------------------------------------------------------------------------
// Command Line Options

#define FALSE  0
#define TRUE   1
#define LASTNUM_MAX  1000

// option vars
const char *opt_infile = NULL;
const char *opt_outfile = NULL;
float opt_ratio = 0.666;
float opt_ratio_delta = 0.05;
int opt_ratio_flag = FALSE;
int opt_dupes_flag = FALSE;
int opt_score = 0;
int opt_lastnum = 100;


int usage(const char *progname) {

  fprintf(stderr, "filterdb (c) 2019 Carl Gorringe (carl.gorringe.org)\n");
  fprintf(stderr, "Usage: %s [options] -i in_mosaic.db -o out_mosaic.db\n", progname);
  fprintf(stderr, "Options:\n"
    //"\t-s <W>x<H>[+<X>+<Y>] : Filter tiles by aspect ratio (e.g. 312x468+5+5).\n"
    "\t-r <ratio>+<delta> : Filter tiles by aspect ratio (w/h) +/- threshold delta. (e.g. 0.666+0.05)\n"
    "\t-d <score>         : Filter dupes within score. (default=0)\n"
    "\t-n <lastnum>       : When filtering dupes, only look back prior <lastnum> tiles. (default=100, max=999)\n"
    "\n"
  );
  return 1;
}

int cmdLine(int argc, char *argv[]) {

  // command line options
  int opt;
  while ((opt = getopt(argc, argv, "?r:d:n:i:o:")) != -1) {
    switch (opt) {
      case '?':  // help
        return usage(argv[0]);
        break;
      case 'r':  // aspect ratio
        opt_ratio_flag = TRUE;
        if (sscanf(optarg, "%f%f", &opt_ratio, &opt_ratio_delta) < 2) {
          fprintf(stderr, "Invalid ratio + delta '%s'\n", optarg);
          return usage(argv[0]);
        }
        break;
      case 'd':  // dupe score
        opt_dupes_flag = TRUE;
        if (sscanf(optarg, "%d", &opt_score) != 1 || opt_score < 0) {
          fprintf(stderr, "Invalid dupe score '%s'\n", optarg);
          return usage(argv[0]);
        }
        break;
      case 'n':  // look back n tiles
        if (sscanf(optarg, "%d", &opt_lastnum) != 1 || opt_lastnum < 1 || opt_lastnum >= LASTNUM_MAX) {
          fprintf(stderr, "Invalid last num of tiles '%s'\n", optarg);
          return usage(argv[0]);
        }
        break;
      case 'i':  // input mosaic db filename
        opt_infile = strdup(optarg); // leaking? ignore?
        break;
      case 'o':  // output mosaic db filename
        opt_outfile = strdup(optarg); // leaking? ignore?
        break;
      default:
        return usage(argv[0]);
    }
  }
  return 0;
}

void die(char* errMsg) {

  fprintf(stderr, "\nERROR: %s\n", errMsg);
  exit(1);
}

//-----------------------------------------------------------------------------
// Compares tile with every tile in pastTiles array.
// Returns the minimum score of best match.

int minTileScore(TileRecord *tile, TileRecord *pastTiles, int pastTilesNum)
                // int numBlocks, int lumFlag=0, int Wy=1, int Wc=1, int We=0)
{
  int i, j, score, Ydelta, minScore = INT_MAX;
  const int lumFlag=0, Wy=1, Wc=1, numBlocks=BLOCKS;  // default flags

  // Loop thru array of pastTiles
  for (i=0; i < pastTilesNum; i++) {
    if (pastTiles[i].magic != TILE_MAGIC) continue;
    score = 0;
    if (lumFlag) {
      // use normalized Y values
      for (j=0; j < numBlocks; j++) {
        score +=  Wy *    abs( pastTiles[i].pixel[j].Y - tile->pixel[j].Y )
              +   Wc * (( abs( pastTiles[i].pixel[j].U - tile->pixel[j].U ) 
                        + abs( pastTiles[i].pixel[j].V - tile->pixel[j].V )) >> 1);
      }
    }
    else {
      // use original Y values
      Ydelta = pastTiles[i].Ydelta - tile->Ydelta;
      for (j=0; j < numBlocks; j++) {
        score +=  Wy *    abs( pastTiles[i].pixel[j].Y - tile->pixel[j].Y + Ydelta )
              +   Wc * (( abs( pastTiles[i].pixel[j].U - tile->pixel[j].U ) 
                        + abs( pastTiles[i].pixel[j].V - tile->pixel[j].V )) >> 1);
      }
    }
    if (score < minScore) { minScore = score; }
  }

  return minScore;
}


//-----------------------------------------------------------------------------
int main(int argc, char *argv[]) {

  // init variables
  int e, i, o, score;
  int numLibTiles, dbSize;
  FILE *INFILE, *OUTFILE;
  time_t timeBegin, timeEnd;
  clock_t clockBegin, clockEnd;
  double clockDiff;
  TileRecord mainTile; 
  TileRecord *pastTiles;  //[LASTNUM_MAX];
  int pastPos = 0;
  int filterFlag = FALSE;
  float ratio;

  // process command line
  if ( (e = cmdLine(argc, argv)) ) { return e; }
  //if (argc != 3) { return usage(argv[0]); }
  if (opt_infile == NULL || opt_outfile == NULL) { return usage(argv[0]); }
  fprintf(stderr, "Running filterdb...\n");

  fprintf(stderr, "  filter aspect:%s  ratio:%.3f  delta:%.3f \n", (opt_ratio_flag ? "yes" : "no"),
    opt_ratio, opt_ratio_delta);
  fprintf(stderr, "  filter dupes: %s  score:%d  lastnum:%d \n", (opt_dupes_flag ? "yes" : "no"),
    opt_score, opt_lastnum);

  // init past tiles array
  if (opt_dupes_flag) {
    pastTiles = (TileRecord *) calloc(opt_lastnum, sizeof(TileRecord));
    if (pastTiles == NULL) die("Could not init pastTiles");
  }

  // read and process every tile in library db file
  fprintf(stderr, "Input tile database: %s \n", opt_infile);

  // get file size of input db
  struct stat statBuf;
  stat(opt_infile, &statBuf);
  dbSize = statBuf.st_size;
  numLibTiles = dbSize / sizeof(TileRecord);
  fprintf(stderr, "  size:%d  numTiles:%d\n", dbSize, numLibTiles);
  if (dbSize < sizeof(TileRecord)) die("Input tile database doesn't exist or is zero size!");

  fprintf(stderr, "Output tile database: %s \n", opt_outfile);

  // open tile databases
  if ((INFILE = fopen(opt_infile, "rb")) == NULL) die("Cannot open input tile database file!");
  if ((OUTFILE = fopen(opt_outfile, "wb")) == NULL) die("Cannot open output tile database file!");

  // measure time
  timeBegin = time(NULL);
  clockBegin = clock();
 
  // loop through every image in tile database
  fprintf(stderr, "Filtering Tiles...\n");
  o=0;
  for (i=0; i < numLibTiles; i++) {

    filterFlag = FALSE;

    // output current tile record number followed by CR to keep cursor on same line
    if ((i % 100) == 0) {
      fprintf(stderr, "in: %d (%.1f%%)  out: %d\r", i, (100.0f * (i+1)/numLibTiles), o);
    }

    // read a tile one at a time
    if ( fread(&mainTile, sizeof(TileRecord), 1, INFILE) != 1 ) die("Problem reading tile record!");

    // process one tile at a time
    if (mainTile.magic != TILE_MAGIC) die("Tile magic number invalid.");

    // filter tile by aspect ratio
    if (opt_ratio_flag) {
      ratio = (float)mainTile.xres / (float)mainTile.yres;
      if (ratio < (opt_ratio - opt_ratio_delta) || ratio > (opt_ratio + opt_ratio_delta)) {
        filterFlag = TRUE;
      }
    }

    // filter dupe tiles by score
    if (opt_dupes_flag) {
      score = minTileScore(&mainTile, pastTiles, opt_lastnum);
      if (score <= opt_score) { filterFlag = TRUE; }

      // copy mainTile into pastTiles
      pastTiles[pastPos] = mainTile;  // check this?
      pastPos++;
      if (pastPos >= opt_lastnum) { pastPos = 0; }
    }

    // output tile to db
    if (filterFlag == FALSE) {
      if ( fwrite(&mainTile, sizeof(TileRecord), 1, OUTFILE) != 1 ) die("Problem writing tile record!");
      o++;
    }

  }

  fprintf(stderr, "\nTiles copied: %d of %d\n", o, i);

  // print time
  clockEnd = clock();
  timeEnd = time(NULL);
  clockDiff = ((double) (clockEnd - clockBegin)) / CLOCKS_PER_SEC;
  fprintf(stderr, "Took %.2f secs. (%.0f secs)\n", clockDiff, difftime(timeEnd, timeBegin) );

  // close databases
  if (fclose(OUTFILE) != 0) die("Cannot close output tile database file!");
  if (fclose(INFILE) != 0) die("Cannot close input tile database file!");

  // free memory
  if (opt_dupes_flag) {
    free(pastTiles); pastTiles = NULL;
  }

  return 0;
}
