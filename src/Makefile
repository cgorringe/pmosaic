# mosaic makefile
# 9/1/2010, 8/12/19

all: mosaic filterdb

mosaic: mosaic.o
	gcc -Wall -O3 mosaic.o -o mosaic -lJudy

mosaic.o:
	gcc -Wall -O3 -c mosaic.c

filterdb: filterdb.o
	gcc -Wall -O3 filterdb.o -o filterdb -lJudy

filterdb.o:
	gcc -Wall -O3 -c filterdb.c

clean:
	rm -f mosaic.o filterdb.o

cleanall:
	rm -f mosaic.o mosaic filterdb.o filterdb

