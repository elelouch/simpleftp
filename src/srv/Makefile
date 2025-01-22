CC := gcc
CFLAGS :=-Wall -g
CFLAGS += -I../socket
LIBS := -lsocketmgmt
LDFLAGS := -L../socket
BINS := ftpsrv.o ftpsrv

BIN_DIR := $(BIN_DIR)


ftpsrv.o: myftpsrv_skel.c 
	$(CC) $(CFLAGS) -c $^ -o $@

ftpsrv: ftpsrv.o
	$(CC) $(CFLAGS) -o ../../bin/$@ $^ $(LIBS) $(LDFLAGS)

all: $(BINS)

clean:
	rm -rf $(BINS)
