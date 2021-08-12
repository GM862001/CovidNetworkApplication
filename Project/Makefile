CFLAGS := -Wall -lpthread
CC := gcc

all: peer ds

peer: peer.c
	$(CC) $(CFLAGS) peer.c -o peer

server: ds.c
	$(CC) $(CFLAGS) ds.c -o ds

clean:
	rm peer ds
