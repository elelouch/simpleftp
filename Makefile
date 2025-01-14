CC=gcc
CFLAGS=-Wall
dir_guard := @mkdir -p ./bin

default: 
	$(dir_guard)
	$(CC) $(CFLAGS) ./src/cli/myftp_skel.c -o ./bin/ftpcli
	$(CC) $(CFLAGS) ./src/srv/myftpsrv_skel.c -o ./bin/ftpsrv
