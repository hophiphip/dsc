CC=gcc
CFLAGS=-lX11 -lXext -pedantic

OUT_BIN=dsc

all:
	$(CC) main.c -o $(OUT_BIN) $(CFLAGS)
