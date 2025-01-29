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

**For details on the usage it is recommended to use the -h flag on the compiled binary.**
```
ftpsrv -h
ftpcli -h
```

If feeling like cleaning the binaries, use clean.
```
make clean
```


## Implementation 

### Client
The client implements the following commands: put, get, ls, cd and pwd.
For using verbose and an active connection, use the flags 'v' and 'A' respectively.

### Server
**Remember to create a 'ftpusers' file with the \<user:password\> list at the same level than the binary**. Use the following command to create one.
```
echo 'YOURUSERNAME:YOURPASSWORD' > ftpusers
```
The server(src/srv) folder contains an example.

The server supports the following commands: STOR, RETR, LIST, CWD, PWD, PASV, PORT.


### Known issues
Overall, the implementation was made for demonstration purposes. 
Security wasn't a concern. 
Also interrupting the processes might left some resources opened, in contrast to
an orderly shutdown (for example, using QUIT).
