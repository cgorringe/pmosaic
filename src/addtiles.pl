#!/usr/bin/perl -w
#------------------------------------------------------------------------------
# addtiles.pl
# Copyright (c) 2010 Carl Gorringe - carl.gorringe.org
#
# Stores data on images to a mosaic library database file.
#
# Version History:
#  (v1.0) 6/6/2010
#
#------------------------------------------------------------------------------
# Status: Almost DONE?
# [ ] still needs edge detection

use strict;
use warnings;

#------------------------------------------------------------------------------
# Print Usage info

sub usage()
{
  print "Add images to a mosaic library. (v1.0) \n";
  print "(c) 2010 Carl Gorringe - carl.gorringe.org\n\n";
  print "Usage: addtiles.pl lib_path imagefiles...\n";
  print "       lib_path is the library directory path to store modified image files.\n";
  print "       imagefiles is one or more image files or directories of files to read.\n\n";
  exit(1);
}

#------------------------------------------------------------------------------
# Recursively retrieves path + filenames from given directory
# (done)

sub getFilenames
{
  my $path = shift;
  my @filelist = ();

  if (-d $path) {
    #print "opening dir $path\n";
    opendir(DH, $path);
    my @filelist2 = sort(readdir(DH));
    closedir(DH);
    foreach my $file (@filelist2) {
      if (substr($file, 0, 1) ne '.') {
        # don't include hidden dot files
        push @filelist, getFilenames($path .'/'. $file);
      }
    }
  }
  elsif (-f $path) {
    #print "file $path \n";
    push @filelist, $path;
  }
  return @filelist;
}

#------------------------------------------------------------------------------
# ** NOT USED **
# Takes as input a string containing RGB data in .ppm format.
# Outputs an array of 192 RGB values: (R1, G1, B1, R2, G1, B2,..., R192, G192, B192)
#
# PPM data is formated as follows:
#   P3
#   8 8
#   255
# Followed by 8*8*3 = 192 RGB values [0-255] separated by space or newline...

sub readPPM
{
  my $data = shift;
  my @rgbData = ();

  @rgbData = split(' ', $data);

  return @rgbData;
}

#------------------------------------------------------------------------------
# Main Program
{
  # Retrieve command line
  if (scalar @ARGV < 2) { usage(); }
  my $libPath = shift @ARGV;
  if (substr($libPath, -1, 1) ne '/') { $libPath .= '/'; }

  # Define Constants
  my $dbTxtFile = $libPath .'mosaic.txt';
  my $dbBinFile = $libPath .'mosaic.db';
  my $dbListFile = $libPath .'filelist.txt';
  my $dbBinRecSize = 270;   # record size is 8*8*4+14 = 270 bytes

  # Retrieve file list
  print "Retrieving file list...\n";
  my @filelist = ();
  foreach my $path (@ARGV) {
    if (substr($path, -1, 1) eq '/') { chop $path; }
    push @filelist, getFilenames($path);
  }

  ## TEST ##
  # foreach (@filelist) {
  #   print "$_\n";
  # }

  # Retrieve last imgID in db
  my $imgID = 1;
  my $dbBinSize = 0;
  if (-e $dbTxtFile) {
    # calculate $imgID based on last id number in db
    # which is more accurate than based on file size
    my $cmdtail = "tail -n1 $dbTxtFile";
    my $tail = `$cmdtail`;
    if ($? != 0) { die "tail command error: $! (errcode $?)"; }
    my @tailNums = split(' ', $tail);
    my $lastId = shift @tailNums;
    $imgID = $lastId + 1;
  }
  elsif (-e $dbBinFile) {
    # else calculate $imgID based on file size
    $dbBinSize = -s $dbBinFile;
    $imgID = int($dbBinSize / $dbBinRecSize) + 1;
  }
  print "Next TileID: $imgID  (= mosaic.db $dbBinSize bytes / $dbBinRecSize + 1)\n";

  # Open library database for appending.
  my ($imgIDstr, $tilePath);
  open(LISTFILE, '>>', $dbListFile) or die "Error appending to file $dbListFile : $!";
  open(DBTXT, '>>', $dbTxtFile) or die "Error appending to text database $dbTxtFile : $!";
  open(DBBIN, '>>', $dbBinFile) or die "Error appending to binary database $dbBinFile : $!";
  binmode DBBIN;  # set file to binary mode

  # Handle SIGINT (ctrl-C)
  my $exitFlag = 0;
  local $SIG{INT} = sub { $exitFlag = 1; };

  # Loop thru every file
  print "Adding ". scalar(@filelist) ." image files to tile database...\n\n";
  my ($cmd, $ppm, $temp);
  foreach my $imgFile (@filelist) {
    last if $exitFlag == 1;

    # Create 8-digit img ID. (max ID is 99,999,999)
    $imgIDstr = sprintf('%08d', $imgID);
    print "$imgIDstr $imgFile";  ## TEST ##
    print LISTFILE "$imgIDstr $imgFile";

    # Create 2-digit subdirectories under $libPath if they do not already exist.
    # Subdir tree has 3 depth (e.g. libPath/00/00/00/00000001bg.jpg) and upto 100 images per dir.
    # TODO: No check done to make sure dir creation works without error

    $tilePath = $libPath . substr($imgIDstr, 0, 2) .'/'. substr($imgIDstr, 2, 2) .'/'. substr($imgIDstr, 4, 2) .'/';
    if (-d ($libPath . substr($imgIDstr, 0, 2) )) {
      if (-d ($libPath . substr($imgIDstr, 0, 2) .'/'. substr($imgIDstr, 2, 2) )) {
        unless (-d $tilePath) {
          mkdir $tilePath;
        }
      }
      else {
        mkdir $libPath . substr($imgIDstr, 0, 2) .'/'. substr($imgIDstr, 2, 2);
        mkdir $tilePath;
      }
    }
    else {
      mkdir $libPath . substr($imgIDstr, 0, 2);
      mkdir $libPath . substr($imgIDstr, 0, 2) .'/'. substr($imgIDstr, 2, 2);
      mkdir $tilePath;
    }

    # Use ImageMagick to crop, resize, save to disk, and output 8x8 avg pixel block RGB data in .ppm format
    # TODO: specify tSize on command line
    my ($rSize, $tSize) = ('512x512', '512x512+0+0');  # default size (square)
    # my ($rSize, $tSize) = ('316x475', '312x468+0+0');  # book, crop size 0.66 (2:3 ratio)
    # my ($rSize, $tSize) = ('356x475', '351x468+0+0');  # book, crop size 0.75 (3:4 ratio)
    # my ($rSize, $tSize) = ('480x360', '430x360+2+0');  # cartoon video, crop size 1.2 (6:5 ratio)

    $cmd = "convert $imgFile -resize '". $rSize ."^' -gravity Center -crop '". $tSize ."' "
          .' \( +clone  -write '. $tilePath . $imgIDstr .'_lg.jpg  +delete  \)'
          .' \( +clone  -resize 25%   -write '. $tilePath . $imgIDstr .'_md.jpg  +delete  \)'
          .' \( +clone  -resize 6.25% -write '. $tilePath . $imgIDstr .'_sm.jpg  +delete  \)'
          ." -identify  -scale '8x8!'  -compress none  -depth 8  ppm:-";
    $ppm = `$cmd`;
    # if ($? != 0) { die "ImageMagick error: $! (errcode $?)"; }
    if ($? != 0) { print "\nImageMagick error: $! (errcode $?)\n"; $exitFlag = 1; last; }

    # print "$ppm \n";  ## TEST ##

    # Extract first line, which is -identify output:
    # "example.jpg JPEG 364x475=>300x475 364x475+32+0 8-bit sRGB 0.030u 0:00.029"
    $ppm =~ s/^(.*\n)//;
    my $identify = $1;
    # print "identify: $identify";  ## TEST ##
    my @idWords = split(' ', $identify);
    my $res = $idWords[2];
    # print "res: $res \n";  ## TEST ##
    $res =~ m/^(\d+)x(\d+)/;
    my ($xres, $yres) = ($1, $2);

    print " \t ${xres}x${yres}\n";
    print LISTFILE " ${xres}x${yres}\n";

    # Remove commented lines beginning with '#' from $ppm (fixes bug)
    $ppm =~ s/\#.*$//mg;
    
    # print "$ppm \n";  ## TEST ##

    # Extract pixel data from .ppm text output.
    my @rgbData = split(' ', $ppm);

    #   PPM data is formatted as follows:
    #     P3
    #     8 8
    #     255
    #   Followed by 8*8*3 = 192 RGB values [0-255] separated by space or newline...

    # Remove first 4 items from ppm data.
    if ((shift @rgbData) ne 'P3')  { die "PPM data not in correct format! (1st != P3)"; }
    if ((shift @rgbData) ne '8')   { die "PPM data not in correct format! (2nd != 8)"; }
    if ((shift @rgbData) ne '8')   { die "PPM data not in correct format! (3rd != 8)"; }
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

    # Calculate $Ydelta
    my $Ydelta = 0;
    my $Ysum = 0;
    for (@Y) { $Ysum += $_; }
    { use integer; $Ydelta = $Ysum / scalar(@Y) - 128; }  # $Ydelta is a signed int [-255,+255]

    # print "Ysum = $Ysum,  Ydelta = $Ydelta \n";  ## TEST ##

    # Normalize the Y values
    my @YN = ();
    while (@Y) {
      $temp = shift @Y;
      $temp -= $Ydelta;
      push @YN, (($temp < 0) ? 0 : (($temp > 255) ? 255 : $temp));  # clips $temp to [0,255]
    }

    # print "Y' = @YN \n";  ## TEST ##

    # TODO: Edge Detection (haven't figured this out precisely yet.)

    # Prepare data for output
    my @outData = ();
    push @outData, $imgID;
    push @outData, $Ydelta;
    push @outData, $xres;
    push @outData, $yres;
    for (my $i=0; $i < scalar(@YN); $i++) {
      push @outData, $YN[$i];
      push @outData, $U[$i];
      push @outData, $V[$i];
      push @outData, 0;       # temporary E value
    }

    #  Output per image: (270 bytes, was 268)
    #    'TILE'   (4-byte ASCII) [only in binary db] 'TILE' = 0x54494C45
    #    imageID  (4-byte long int)
    #    Ydelta   (2-byte signed short int)
    #    xres     (2-byte signed short int)
    #    yres     (2-byte signed short int)
    #    matrixD  (8*8*4 bytes) which contain average values {Y', U, V, E}

    # Output to database text file
    my $outLine = join(' ', @outData) ."\n";
    # print $outLine;   ## TEST ##
    print DBTXT $outLine;

    # Output to database binary file
    my $binLine = pack('CCCC', ord('T'), ord('I'), ord('L'), ord('E'));  # 'TILE'
    $binLine   .= pack('l', shift @outData);  # imgID
    $binLine   .= pack('s', shift @outData);  # Ydelta
    $binLine   .= pack('s', shift @outData);  # xres
    $binLine   .= pack('s', shift @outData);  # yres
    $binLine   .= pack('C[256]', @outData);   # 8*8*4 = 256  # TODO: Don't hardcode this?
    # tried replacing 256 with '*', but error msg!  Official docs incorrect for perl v5.10.0
    # see: http://perldoc.perl.org/functions/pack.html
    print DBBIN $binLine;

    $imgID++;
  }  # next imgFile

  close DBBIN     or die "Can't close $dbBinFile: $!\n";
  close DBTXT     or die "Can't close $dbTxtFile: $!\n";
  close LISTFILE  or die "Can't close $dbListFile: $!\n";

  if ($exitFlag == 1) {
    print "Exited early. Next TileID: $imgID\n";
  }
  print "\nDone!\n";  ## TEST ##
}

# EOF
