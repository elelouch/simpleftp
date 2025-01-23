# SimpleFTP

Network sockets assignment made for a computer network course.

## Dependencies
- gcc
- make

## Source code organization

```
src --- cli
     |
     |- srv
     |
      - libsocket

```
- cli: Source code for the client.
- srv: Source code for the server.
- libsocket: Static library shared by both server and client.

## How to compile the code
Use the following command inside the repository.
```
make
```

Now, each directory from the source code should have it's respective binary (client example: src/cli/ftpcli).
For details on the usage, use the -h flag on the binary.
```
ftpsrv -h
ftpcli -h
```

If feeling like cleaning the binaries, for example, for testing purposes. Use clean.
```
make clean
```


## Implementation 

### Client
The client implements the following commands: put, get, ls, cd and pwd.

### Server
The server supports the following commands: STOR, RETR, LIST, CWD, PWD, PASV, PORT.

### Known issues
Overall, the implementation was made for demonstration purposes. 
Security wasn't a concern. 
Also interrupting the processes might left some resources opened, in contrast of
an orderly shutdown (by using QUIT, for example).
