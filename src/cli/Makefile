CC=gcc
CFLAGS=-Wall -g 
CFLAGS += -I ../socket
LDFLAGS= -L../socket
LIBS= -lsocketmgmt # this is similar to -lc which translates to libc
TARGET= ftpcli

BINS = ftpcli.o $(TARGET)

ftpcli.o: myftp_skel.c 
	$(CC) $(CFLAGS) -c $^ -o $@


$(TARGET): ftpcli.o
	$(CC) $(CFLAGS) -o $(TARGET) $^ $(LIBS) $(LDFLAGS)

clean: 
	rm -rf $(BINS)

all: $(BINS)
