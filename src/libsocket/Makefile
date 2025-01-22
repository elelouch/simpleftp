CC=gcc
CFLAGS=-Wall -g

BINS=libsocketmgmt.so libsocketmgmt.o libsocketmgmt.a

all: $(BINS)

# Already creates lisocketmgmt.o
libsocketmgmt.o: libsocketmgmt.c socketmgmt.h  
	$(CC) $(CFLAGS) -c libsocketmgmt.c

# Dynamic library. Advantages: Modular and less size.
# fPIC: file Position Independent Code 
# .so: shared object
# lc:  ??
# shared objects are designed to be loaded at runtime
libsocketmgmt.so: libsocketmgmt.c socketmgmt.h
	$(CC) $(CFLAGS) -fPIC -shared -o $@ libsocketmgmt.c

# Static library: Self contained
# rcs: (r)eplace (c)reate and (s) for index
libsocketmgmt.a: libsocketmgmt.o
	ar rcs $@ $^

static: libsocketmgmt.a

clean: 
	rm -rf $(BINS)

