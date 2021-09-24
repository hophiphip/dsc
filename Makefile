CC=gcc
CFLAGS=-lX11 -pedantic

OUT_BIN=sc

all:
	$(CC) main.c -o $(OUT_BIN) $(CFLAGS)
