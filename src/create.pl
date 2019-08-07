#!/usr/bin/perl -w
#------------------------------------------------------------------------------
# create.pl
# Copyright (c) 2010 Carl Gorringe - carl.gorringe.org
#
# Takes CSV output from mosaic.c and creates a photomosaic.
#
# Version History:
#  (v1.0) 9/5/2010
#
#------------------------------------------------------------------------------
# Status: NOT DONE
#
# TODO:
# [+] Reorder tiles based on position. (NOT TESTED)
# [+] insert blank tile for missing position numbers?  (NOT TESTED)
# [ ] Vertical Flip Tiles

use strict;
use warnings;

#------------------------------------------------------------------------------
# Print Usage info

sub usage()
{
  print STDERR "Creates image from mosaic program's CSV output. (v1.0) \n";
  print STDERR "(c) 2010 Carl Gorringe - carl.gorringe.org\n\n";
  print STDERR "Usage: create.pl lib_path [png|html] [sm|med|lg] < input.csv > output \n";
  print STDERR "       lib_path is the root library path where tile images are located.\n";
  print STDERR "       'png' or 'html' is type of output file to create.\n";
  print STDERR "       'sm', 'med', or 'lg' to specify tile size.\n";
  print STDERR "       input.csv is the CSV file from mosaic's output.\n";
  print STDERR "       output is the html or png image file to create.\n\n";
  exit(1);
}

#------------------------------------------------------------------------------
# Main Program
{
  # my %tileSizeExt = ( 'sm' => 'x32.png', 'med' => 'x128.jpg', 'lg' => 'x480.jpg' );
  my %tileSizeExt = ( 'sm' => '_sm.jpg', 'med' => '_md.jpg', 'lg' => '_lg.jpg' );
  my ($i, $x, $y);

  #--- Retrieve command line ---
  if (scalar @ARGV != 3) { usage(); }
  print STDERR "Running $0\n";  ## TEST ##

  my $libPath = shift @ARGV;
  if (substr($libPath, -1, 1) ne '/') { $libPath .= '/'; }
  my $outType = lc(shift @ARGV);
  my $sizeType = lc(shift @ARGV);
  print STDERR "  lib path: $libPath   output:$outType  size:$sizeType\n";  ## TEST ##
  # print STDERR "  output: $outType  size: $sizeType\n";  ## TEST ##

  if (not exists $tileSizeExt{$sizeType}) { print STDERR "Invalid size!\n"; usage(); }

  #--- Read input.csv ---
  my $line = <STDIN>;
  chomp $line;
  my ($xTiles, $yTiles, $xBlocks, $yBlocks, $lumFlag, $Wy, $Wc, $We, $dups, undef) = split(',', $line);
  print STDERR "Creating ${xTiles}x${yTiles} tiled photomosaic with up to $dups duplicate tiles.\n";  ## TEST ##

  # init blank tiles
  my ($pos, $Ydelta, $imgID, $imgIDstr, $tileFile, $t1, $t2, $vflipFlag);
  my @imgFiles = ();
  my @imgBigFiles = ();
  my $numTiles = $xTiles * $yTiles;
  $t1 = '00/00/00/00000000' . $tileSizeExt{$sizeType};
  $t2 = '00/00/00/00000000' . $tileSizeExt{'lg'};
  for ($i=0; $i < $numTiles; $i++) {
    push @imgFiles, $t1;
    push @imgBigFiles, $t2;
  }

  while ($line = <STDIN>) {
    chomp $line;
    ($pos, $Ydelta, $imgID, undef) = split(',', $line);

    # TODO: if $imgID is negative, flip image vertically
    if ($imgID < 0) {
      $imgID *= -1; 
      $vflipFlag = 1;
    }
    else {
      $vflipFlag = 0;
    }

    # Create file path + name from imgID (e.g. "00/00/00/00000001x32.png")
    $imgIDstr = sprintf('%08d', $imgID);
    $tileFile = substr($imgIDstr, 0, 2) .'/'. substr($imgIDstr, 2, 2) .'/'. 
                substr($imgIDstr, 4, 2) .'/'. $imgIDstr;
    # push @imgFiles, $tileFile . $tileSizeExt{$sizeType};
    # push @imgBigFiles, $tileFile . $tileSizeExt{'lg'};
    $imgFiles[$pos] = $tileFile . $tileSizeExt{$sizeType};
    $imgBigFiles[$pos] = $tileFile . $tileSizeExt{'lg'};

    # print STDERR "$tileFile\n";  ## TEST ##
  }

  #--- Output photomosaic ---
  if ($outType eq 'png') {
    #-- Create PNG image --
    # NOTE: How to create PNG using ImageMagick:
    #  echo "img1.png im2.png ... imgN.png" | montage @-  -mode Concatenate  -tile 20x30  png:-

    my $cmd = 'montage  @-  -mode Concatenate  -tile '. ${xTiles} .'x'. ${yTiles} .'  png:-';
    print STDERR "Running: $cmd \n";

    my $timeBegin = time;
    open(PNG, "| $cmd") || die "ERROR: Can't fork to ImageMagick: $! (errcode $?)";
    local $SIG{PIPE} = sub { die "ERROR: ImageMagick pipe broke." };

    foreach $tileFile (@imgFiles) {
      # print "${libPath}${tileFile} ";
      print PNG "${libPath}${tileFile} ";
    }

    close PNG || die "ERROR: Closing ImageMagick pipe: $! (errcode $?)";
    my $timeEnd = time;
    print STDERR sprintf( "Montage took %.0f secs.\n", $timeEnd - $timeBegin );
    print STDERR "Done montage.\n";

  }
  elsif ($outType eq 'html') {
    #-- Create HTML file --
    print "<html><head><title>Photomosaic</title></head><body>\n";
    # print "<h2>Photomosaic</h2><br/>\n";
    print '<table border="0" cellpadding="0" cellspacing="0">'."\n";

    $i = 0;
    for ($y=0; $y < $yTiles; $y++) {
      print " <tr>\n";
      for ($x=0; $x < $xTiles; $x++) {
        print '  <td>';
        print '<a href="'. $imgBigFiles[$i] .'"><img src="'. $imgFiles[$i] .'" border="0"/></a>';
        print "</td>\n";
        $i++;
      }
      print " </tr>\n";
    }

    print "</table>\n";
    print "</body></html>\n";
  }
  else {
    usage();
  }

  print STDERR "Done $0\n\n";  ## TEST ##
}

# EOF
