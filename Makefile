CC=gcc
CFLAGS=-Wall

.PHONY: clean all 

all: socketlib cli srv 

socketlib:
	$(MAKE) -C src/libsocket static

cli:
	$(MAKE) -C src/cli all

srv:
	$(MAKE) -C src/srv all

clean: 
	$(MAKE) -C src/libsocket clean
	$(MAKE) -C src/cli clean
	$(MAKE) -C src/srv clean
