## Photomosaic Image Generator

By [Carl Gorringe](http://carl.gorringe.org)

This is my photomosaic image generator. I first conceived of the algorithm and wrote a program to generate photomosaics using Visual Basic in 1998. I later revisited the idea, improved the algorithm, and entirely rewrote the code as a collection of command-line programs written in Perl and C in 2010.

If you would like to use my code for a project, please [contact me](https://carl.gorringe.org/contact)!

See more photos on my [Photomosaics website](https://carl.gorringe.org/mosaics/).

| Original | Mosaic | Closeup |
| -------- | ------ | ------- |
![](mosaics/archive1-logo.png) | ![](mosaics/archive1-mosaic.jpg) | ![](mosaics/archive1-closeup.jpg)
![](mosaics/archive2-building.jpg) | ![](mosaics/archive2-mosaic.jpg) | ![](mosaics/archive2-closeup.jpg)


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

* [Judy C library](http://judy.sourceforge.net) - used for efficient data structures. See [Judy array](https://en.wikipedia.org/wiki/Judy_array) for more info.
* [ImageMagick](https://imagemagick.org/) - used for graphics processing.

#### macOS using Homebrew

The Judy library appears to have been [removed](https://github.com/Homebrew/homebrew-core/issues/1562) from Homebrew so you'll need to compile from source, which I've included in the **zips** directory or you can download it from the above link. See instructions below. Then install ImageMagick:

```
brew install imagemagick
```

#### Ubuntu / Debian Linux

You may need to add the *universe* repo in order to install `libjudy-dev` (*need to check*)

```
apt-get install libjudy-dev imagemagick
```

#### Installing Judy library from Source

In case this library may be hard to find in the future, I've included it in the **zips** directory. Taking the following steps should compile and install the library. See the archive's included INSTALL and README files for details.

```
tar -xf Judy-1.0.5.tar.gz
cd judy-1.0.5
./configure
make
make check
sudo make install
```


### How to Use

#### Steps:

1. First run ```make``` from the src directory to compile the C source code.
2. Run ```./addtiles.pl``` to create an image library database.
3. Create a shell script (below), modified for your use.
4. Run the shell script from inside the **src** directory.

#### Example Shell Script:

```
#!/bin/sh
echo "Creating Photomosaic"
./masterimg.pl 20 15 1 ../../input/photo.jpg | ./mosaic ../../lib/mosaic.db > ../../output/mosaic1.txt
./create.pl ../../lib/ png med < ../../output/mosaic1.txt > ../../output/mosaic1.png

```


### What Programs Do

1. **addtiles.pl** - Add image tiles to a mosaic library. Need to do this first before trying to generate a photomosaic. It can take a long time with a large collection, but it only has to be done once. Expect several hours or run overnight depending on size of collection. With modern computers, this step may complete within minutes.

2. **masterimg.pl** - This extracts color data from the master image and produces a CSV text file to be used as input into the **mosaic** program. You tell this program how many tiles that you want across and down in the photomosaic, and how many duplicate tiles to allow. Takes a few seconds.

3. **mosaic** - This is the core algorithm that analyzes and matches tiles from the tile database to the master image. It produces a CSV text file representing the positions of all the tile images in the final photomosaic image, which is then fed into **create.pl**. Should only take a few minutes.

4. **create.pl** - Takes the output from **mosaic** to generate the final photomosaic image. You can choose from 3 sizes, and either a PNG image, or an HTML table. Uses ImageMagick to generate the PNG, which is a little slow and can take up to a few minutes. The HTML generator is faster.

Programs 2, 3, & 4 are designed to take input and produce output to standard in/out, which means you can pipe the output of one to another.


### How to Create a Photomosaic from Video Stills

First install the **ffmpeg** command-line tool. On macOS using Homebrew you can do this:

```
brew install ffmpeg
```

To export an image for every frame in the video, run something like this:

```
mkdir frames
ffmpeg -i input.mp4 frames/output_%06d.jpg
```

If your video is 30 fps, then the number of output images will be 30 * length of video in seconds. If you only want to capture a single frame per second, you can do this:

```
ffmpeg -i input.mp4 -r 1 frames/output_%06d.jpg
```

Replace `-r 1` with `-r 1/2` for 2 frames per second, `-r 1/3` for 3 per second, etc.

After you've got your images, run `addtiles.pl` to import them into an image library.


____________________________________________________________

## License

Copyright (c) 2010-2019 Carl Gorringe. All rights reserved.
