CC=gcc
CFLAGS=-Wall -g

BINS=libsocketmgmt.so libsocketmgmt.o

all: $(BINS)

# Already creates lisocketmgmt.o
libsocketmgmt.o: libsocketmgmt.c libsocketmgmt.h  
	$(CC) $(CFLAGS) -c libsocketmgmt.c

# fPIC: file Position Independent Code 
# .so: shared object
# lc:  ??
# shared objects are designed to be loaded at runtime
libsocketmgmt.so: libsocketmgmt.c 
	$(CC) $(CFLAGS) -fPIC -shared -o $@ libsocketmgmt.c -lc

clean: 
	rm -rf *.o *.so 

all: libsocketmgmt.o
