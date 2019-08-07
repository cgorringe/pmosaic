## Photomosaic

By [Carl Gorringe](http://carl.gorringe.org)

This is my photomosaic image generator. I first conceived of the algorithm and wrote a program to generate photomosaics using Visual Basic in 1998. I later revisited the idea, improved the algorithm, and entirely rewrote the code as a collection of command-line programs written in Perl and C in 2010. I'm working on updating the code and releasing as open-source in 2019.

### Requirements

* This code has been tested under Mac OS X and Ubuntu Linux. It hasn't been tested under Windows.

* You'll want a large collection of photos to use as tiles. I recommend at least 10K, but 100K or more is better. There are other ways of generating photos, such as still frames from a video, which you can try. I may eventually write up some ways here in the future.

### Pre-Install

When cloning the code, please place inside a parent directory, which you can then create a few more directories under. For example, you can create a **mosaics** directory, which will be the root, then within this check out the code:

```
  git clone https://github.com/cgorringe/pmosaic
```

Then create these directories inside the root:

```
  mkdir lib input output scripts
```

* **lib** contains the mosaic database (mosaic.db) and copies of any photos added, so make sure to have plenty of room on your drive. You can have multiple **lib** directories and name them however you want, one for each collection of tiles.
* **input** is where you place your master photos, if you want. You can also name this however you want.
* **output** is where the program's output text files and final mosaic images go. Name however you like.
* **scripts** is where to place your mosaic generation scripts, which you'll see examples below.


### Install Dependencies

* Judy C library
* ImageMagick

#### 1. Judy C library

**Mac OS X using Homebrew:**

```
brew install judy
```

**Ubuntu or Debian Linux:**

May need to add the *universe* repo in order to install this. (*need to check*)

```
apt-get install libjudy-dev
```

#### 2. ImageMagick

TODO


### How to Use

**This is not complete yet.** (will finish later...)

1. First run ```make``` from the src directory to compile the C source code.
2. Run ```./addtiles.pl``` to create an image library database.
3. Create a shell script (below), modified for your use.
4. Run the shell script from inside the **src** directory.

```
#!/bin/sh
echo "Creating Photomosaic"
./masterimg.pl 20 15 1 ../../input/photo.jpg | ./mosaic ../../lib/mosaic.db > ../../output/mosaic1.txt
./create.pl ../../lib/ png med < ../../output/mosaic1.txt > ../../output/mosaic1.png

```

#### What Programs Do

1. **addtiles.pl** - Add image tiles to a mosaic library. Need to do this first before trying to generate a photomosaic. It can take a long time with a large collection, but it only has to be done once. Expect several hours or run overnight depending on size of collection.

2. **masterimg.pl** - This extracts color data from the master image and produces a CSV text file to be used as input into the **mosaic** program. You tell this program how many tiles that you want across and down in the photomosaic, and how many duplicate tiles to allow. Takes a few seconds.

3. **mosaic** - This is the core algorithm that analyzes and matches tiles from the tile database to the master image. It produces a CSV text file representing the positions of all the tile images in the final photomosaic image, which is then fed into **create.pl**. Should only take a few minutes.

4. **create.pl** - Takes the output from **mosaic** to generate the final photomosaic image. You can choose from 3 sizes, and either a PNG image, or an HTML table. Uses ImageMagick to generate the PNG, which is a little slow and can take up to a few minutes. The HTML generator is faster.

Programs 2, 3, & 4 are designed to take input and produce output to standard in/out, which means you can pipe the output of one to another.


## License

Copyright (c) 2010-2019 Carl Gorringe. All rights reserved.
