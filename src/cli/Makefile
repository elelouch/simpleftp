CC=gcc
CFLAGS=-Wall -g 
CFLAGS += -I ../socket

ftpcli.o: myftp_skel.c 
	$(CC) $(CFLAGS) -c $^ -o ftpcli.o 

ftpcli: ftpcli.o
	$(CC) $(CFLAGS) -o $@ $^ -lsocketmgmt -L../socket

all: ftpcli
