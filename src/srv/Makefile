CC := gcc
CFLAGS :=-Wall -g
CFLAGS += -I../libsocket
LIBS := -lsocketmgmt
LDFLAGS := -L../libsocket
BINS := ftpsrv.o ftpsrv


ftpsrv.o: myftpsrv_skel.c 
	$(CC) $(CFLAGS) -c $^ -o $@

ftpsrv: ftpsrv.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS) $(LDFLAGS)

all: $(BINS)

clean:
	rm -rf $(BINS)
