Welcome to the Pa1 application set.
The Pa1 application set includes client and server programs which demonstrate data transmission scenarios.
Included in the folder with the source code for these programs is a compile.sh script which when run in the same folder as the client.cpp and server.cpp files will compile both into usable executables (named client and server, respectively).
The compiled client and server applications should then be run on seperate machines so that their function can be observed.
The client program must recieve the following six arguments when run: 
1.  serverPort: server's port number 
2.  serverName: server's IP address or host name 
3.  iterations: the number of iterations a client performs on data transmission using 
"single write", "writev" or "multiple writes".  
4.  nbufs: the number of data buffers 
5.  bufsize: the size of each data buffer (in bytes) 
6.  type: the type of transfer scenario: 1, 2, or 3 
type corresponds to the write method used, respectively , multi-writes (send each buffer individually), writev, and single-write (send all buffers at once)
The server program must recieve the following two arguments when run:
1.  port: server's port number 
2.  iterations: the number of iterations of client's data transmission activities. This 
value should be the same as the "iterations" value used by the client.