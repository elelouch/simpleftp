CC := gcc
CFLAGS := -Wall -g 
CFLAGS += -I ../libsocket
LDFLAGS := -L../libsocket
LIBS := -lsocketmgmt # this is similar to -lc which translates to libc

TARGETS := ftpcli ftpcli.o

ftpcli.o: myftp_skel.c 
	$(CC) $(CFLAGS) -c $^ -o $@


ftpcli: ftpcli.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS) $(LDFLAGS)

clean: 
	rm -rf $(TARGETS)

all: $(TARGETS)
