#include <iostream>
#include <fstream>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>   // htonl, htons, inet_ntoa 
#include <arpa/inet.h>    // inet_ntoa 
#include <netdb.h>        // gethostbyname 
#include <unistd.h>       // read, write, close 
#include <sys/time.h>

int MAXLINE = 516, PORT = 51949, RETRIES = 10, TIMEOUT = 2;