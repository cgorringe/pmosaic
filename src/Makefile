# mosaic makefile
# 9/1/2010

all: mosaic

mosaic: mosaic.o
	gcc -Wall -O3 mosaic.o -o mosaic -lJudy

mosaic.o:
	gcc -Wall -O3 -c mosaic.c

clean:
	rm -f mosaic.o

cleanall:
	rm -f mosaic.o mosaic

