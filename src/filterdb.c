/*-----------------------------------------------------------------------------
  filterdb.h
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

#include <Judy.h>   // requires libjudy installed, and compile flag -lJudy
#include "mosaic.h"

//-----------------------------------------------------------------------------
// Command Line Options

// option vars
//const char *opt_hostname = NULL;
//int opt_layer  = Z_LAYER;

int usage(const char *progname) {

  fprintf(stderr, "filterdb (c) 2019 Carl Gorringe (carl.gorringe.org)\n");
  fprintf(stderr, "Usage: %s [options] in_mosaic.db out_mosaic.db\n", progname);
  fprintf(stderr, "Options:\n"
    //"\t-s <W>x<H>[+<X>+<Y>] : Filter tiles by aspect ratio (e.g. 312x468+5+5).\n"
    "\t-r <ratio>+<delta> : Filter tiles by aspect ratio (w/h) +/- threshold delta. (e.g. 0.75+0.05)\n"
    "\t-d <score>         : Filter dupes within score. (default=0)\n"
    "\t-n <last-num>      : When filtering dupes, only look back prior <last-num> tiles. (default=100)\n"
  );
  return 1;
}

int cmdLine(int argc, char *argv[]) {

    // command line options
    int opt;
    while ((opt = getopt(argc, argv, "?g:l:t:r:h:d:c:b:n:")) != -1) {
        switch (opt) {
        case '?':  // help
            return usage(argv[0]);
            break;
        case 'g':  // geometry
            if (sscanf(optarg, "%dx%d%d%d", &opt_width, &opt_height, &opt_xoff, &opt_yoff) < 2) {
                fprintf(stderr, "Invalid size '%s'\n", optarg);
                return usage(argv[0]);
            }
            break;
        case 'l':  // layer
            if (sscanf(optarg, "%d", &opt_layer) != 1 || opt_layer < 0 || opt_layer >= 16) {
                fprintf(stderr, "Invalid layer '%s'\n", optarg);
                return usage(argv[0]);
            }
            break;
        case 't':  // timeout
            if (sscanf(optarg, "%lf", &opt_timeout) != 1 || opt_timeout < 0) {
                fprintf(stderr, "Invalid timeout '%s'\n", optarg);
                return usage(argv[0]);
            }
            break;
        case 'r':  // respawn
            if (sscanf(optarg, "%lf", &opt_respawn) != 1 || opt_respawn < 0) {
                fprintf(stderr, "Invalid respawn '%s'\n", optarg);
                return usage(argv[0]);
            }
            break;
        case 'h':  // hostname
            opt_hostname = strdup(optarg); // leaking. Ignore.
            break;
        case 'd':  // delay
            if (sscanf(optarg, "%d", &opt_delay) != 1 || opt_delay < 1) {
                fprintf(stderr, "Invalid delay '%s'\n", optarg);
                return usage(argv[0]);
            }
            break;
        case 'c':  // forground color
            if (sscanf(optarg, "%02x%02x%02x", &opt_fg_R, &opt_fg_G, &opt_fg_B) != 3) {
                opt_fg_R=0, opt_fg_G=0, opt_fg_B=0;
                //fprintf(stderr, "Color parse error\n");
                //return usage(argv[0]);
            }
            opt_fgcolor = true;
            break;
        case 'b':  // background color
            if (sscanf(optarg, "%02x%02x%02x", &opt_bg_R, &opt_bg_G, &opt_bg_B) != 3) {
                opt_bg_R=1, opt_bg_G=1, opt_bg_B=1;
            }
            opt_bgcolor = true;
            break;
        case 'n':  // init with 1/n number of dots
            if (sscanf(optarg, "%d", &opt_num_dots) != 1 || opt_num_dots < 2) {
                fprintf(stderr, "Invalid number of dots '%s'\n", optarg);
                return usage(argv[0]);
            }
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
int main(int argc, char *argv[]) {

  // parse command line
  if (int e = cmdLine(argc, argv)) { return e; }

  return 0;
}
