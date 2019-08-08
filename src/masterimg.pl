#!/usr/bin/perl -w
#------------------------------------------------------------------------------
# masterimg.pl
# Copyright (c) 2010 Carl Gorringe - carl.gorringe.org
#
# Takes an image, outputs CSV of tile data for input to the mosaic program.
# Uses ImageMagick
#
# Usage: 
#   masterimg.pl xtiles ytiles dups image > output.csv 
#
# Version History:
#  (v1.0) 6/6/2010 addtiles.pl
#  (v1.0) 9/4/2010 masterimg.pl
#
# TODO:
# [ ] May need to rotate some source images which aren't rotated.
# [ ] Update command line options to use GetOpt(sp?)
#
#------------------------------------------------------------------------------
# Status: Almost DONE

use strict;
use warnings;

=for comment
CSV Output:

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
    (Y is normalized. Add Ydelta to Y to get actual value.)

=cut

#------------------------------------------------------------------------------
# Print Usage info

sub usage()
{
  print "Creates CSV of master image to use in a photomosaic. (v1.0) \n";
  print "(c) 2010 Carl Gorringe - carl.gorringe.org\n\n";
  print "Usage: masterimg.pl xtiles ytiles dups image > output.csv \n";
  print "       xtiles is number of tiles across. (e.g. 20)\n";
  print "       ytiles is number of tiles down.   (e.g. 30)\n";
  print "       dups is number of max duplicate tiles. (e.g. 1 or 600)\n";
  print "            (negate to include vertically flipped tiles. TEMP)\n";
  print "       image is master image. Can be any supported type. (png, jpg, etc.)\n";
  print "       output.csv is text file that can be used as input to mosaic program.\n\n";
  exit(1);
}

#------------------------------------------------------------------------------
# Main Program
{
  my ($i, $j);

  # Retrieve command line
  if (scalar @ARGV != 4) { usage(); }
  print STDERR "Running $0\n";  ## TEST ##

  my $xTiles = shift @ARGV;
  my $yTiles = shift @ARGV;
  my $dups = shift @ARGV;
  my $imgFile = shift @ARGV;

  my $flags = 0;   #  0x01 = lumFlag, 0x02 = vflipFlag
  if ($dups < 0) {
    # temporary solution to specify vertically flipped tiles.
    # TODO: need to change command-line structure
    $dups = abs($dups);
    $flags = 2;
  }
  my $numTiles = $xTiles * $yTiles;
  my $blocks = 8;
  my $xSize = ($blocks * $xTiles);
  my $ySize = ($blocks * $yTiles);

  my $outSize = sprintf("%d%s%d", $xSize, "x", $ySize);

  print STDERR "Reading: $imgFile\n";
  print STDERR "  output size: $outSize  (${xTiles}x${yTiles} tiles, ${blocks}x${blocks} pixels per tile)  flags:$flags\n";

  # Use ImageMagick to crop, resize, and output (8x)*(8y) avg pixel block RGB data in .ppm format
  # my $cmd = "convert $imgFile -scale '${outSize}^' -gravity Center -crop '${outSize}+0+0' "
  #          ." -compress none  -depth 8  ppm:-";
  my $cmd = "convert -auto-orient $imgFile -scale '${outSize}!'  -compress none  -depth 8  ppm:-";
  my $ppm = `$cmd`;
  if ($? != 0) { die "ImageMagick error: $! (errcode $?)"; }

  # print "$ppm \n";  ## TEST ##
  print STDERR "Converting to YUV...\n";

  # Remove commented lines beginning with '#' from $ppm (fixes bug)
  $ppm =~ s/\#.*$//mg;
    
  # Extract pixel data from .ppm text output.
  my @rgbData = split(' ', $ppm);

  #   PPM data is formatted as follows:
  #     P3
  #     x y
  #     255
  #   Followed by x*y*3 RGB values [0-255] separated by space or newline...

  # Remove first 4 items from ppm data.
  if ((shift @rgbData) ne 'P3')  { die "PPM data not in correct format! (1st != P3)"; }
  if ((shift @rgbData) ne "$xSize")   { die "PPM data not in correct format! (2nd != $xSize)"; }
  if ((shift @rgbData) ne "$ySize")   { die "PPM data not in correct format! (3rd != $ySize)"; }
  if ((shift @rgbData) ne '255') { die "PPM data not in correct format! (4th != 255)"; }

  # print "@rgbData \n";  ## TEST ##

  # Convert RGB values to YUV
  my @Y = ();
  my @U = ();
  my @V = ();
  my ($r, $g, $b);
  while (@rgbData) {
    $r = shift @rgbData;
    $g = shift @rgbData;
    $b = shift @rgbData;
    {
      use integer;
      push @Y, ((  66*$r + 129*$g +  25*$b + 128 ) / 256) + 16;    # Y range is [16,235]
      push @U, (( -38*$r -  74*$g + 112*$b + 128 ) / 256) + 128;   # U range is [0, 255]
      push @V, (( 112*$r -  94*$g -  18*$b + 128 ) / 256) + 128;   # V range is [0, 255]
    }
  }

  # print "Y = @Y \n";  ## TEST ##
  # print "U = @U \n";  ## TEST ##
  # print "V = @V \n";  ## TEST ##

  # Divide image into 8x8 blocks * (xtiles * ytiles)
  # init 2-D arrays [tile][block]
  my @bY = ();
  my @bU = ();
  my @bV = ();
  for ($i=0; $i < $numTiles; $i++) {
    $bY[$i] = ();
    $bU[$i] = ();
    $bV[$i] = ();
  }
  # scan thru every pixel and store into each tile
  my $tile = 0;
  my $bx = $blocks;
  my $by = $blocks;
  my $x = $xSize;
  for ($i=0; $i < scalar(@Y); $i++) {
    push @{ $bY[$tile] }, $Y[$i];
    push @{ $bU[$tile] }, $U[$i];
    push @{ $bV[$tile] }, $V[$i];
    $bx--;
    if ($bx == 0) {
      $bx = $blocks;
      $tile++;
    }
    $x--;
    if ($x == 0) {
      $x = $xSize;
      $by--;
      if ($by == 0) {
        $by = $blocks;
      }
      else {
        $tile -= $xTiles;
      }
    }
  }  # next $i

  ## TEST ##
  # for ($i=0; $i < $numTiles; $i++) {
  #   print "bY[$i] = @{$bY[$i]} \n";
  #   print "bU[$i] = @{$bU[$i]} \n";
  #   print "bV[$i] = @{$bV[$i]} \n";
  # }
  ##########

  # TODO:
  # [ ] Calculate Ydeltas, and normalize Y values.
  # [ ] Calculate Edge values.
  # [ ] Sort output order based on sum of Edge values.
  # [+] Allow setting of VFlipFlag (Flags += 0x02)

  # Output CSV:
  #   Xtiles, Ytiles, Xblocks, Yblocks, Flags, Wy, Wc, We, dups
  #   pos1, Ydelta1, Y1, U1, V1, E1, Y2, U2, V2, E2, ... Yn, Un, Vn, En
  # note: pos is 0-based

  print STDERR "Outputting CSV...\n";

  print "$xTiles,$yTiles,$blocks,$blocks,$flags,1,1,0,$dups\n";
  for ($i=0; $i < $numTiles; $i++) {
    print "$i,0";
    for ($j=0; $j < scalar( @{$bY[$i]} ); $j++) {
      print ",$bY[$i][$j],$bU[$i][$j],$bV[$i][$j],0";
    }
    print "\n";
  }

  print STDERR "Done $0\n\n";  ## TEST ##
}

# EOF
