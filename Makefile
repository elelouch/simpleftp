CC=gcc
CFLAGS=-Wall
dir_guard := @mkdir -p ./bin

BIN_DIR := ./bin

.PHONY: clean all 

all: $(BIN_DIR) socketlib cli srv 

$(BIN_DIR):
	mkdir -p $(BIN_DIR)/files

socketlib:
	$(MAKE) -C src/socket static

cli:
	$(MAKE) -C src/cli all

srv:
	$(MAKE) -C src/srv all

clean: 
	$(MAKE) -C src/socket clean
	$(MAKE) -C src/cli clean
	$(MAKE) -C src/srv clean
	rm -rf $(BIN_DIR)
