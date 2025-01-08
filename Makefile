dir_guard := @mkdir -p ./bin

default: 
	$(dir_guard)
	$(CC) -Wall ./src/cli/myftp_skel.c -o ./bin/ftpcli
