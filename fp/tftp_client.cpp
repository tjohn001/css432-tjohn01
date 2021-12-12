/*
* Taylor Johnston
* Client side application that sends a given number of iterations of 1500 bytes seperated into given buffer sizes using one of 3 methods
* total buffer size adds up to 1500 bytes to given port on a server, then recieves number of read operations the server performed
*/
#include <iostream>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/time.h>


using namespace std;

//method for handling creating connection to server
//takes in port #, server address, number of iterations to run, number of buffers, size of buffers, and the type of sending method to use
int createConnection(const char* port, const char* address, const char* filename) {

    //setup server socket
    struct addrinfo hints;
    struct addrinfo* res;
    int status;

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM; // TCP stream sockets

    //add server address info to res
    if ((status = getaddrinfo(address, port, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return 1;
    }

    int sd = socket(res->ai_family, res->ai_socktype, res->ai_protocol); //socket file descriptor

    //connect to socket
    if (connect(sd, res->ai_addr, res->ai_addrlen) == -1) {
        cerr << "Connection error";
        return 1;
    }

    //deallocate buffer
    for (int i = 0; i < nbufs; i++) {
        delete[] databuf[i];
    }
    delete[] databuf;
    freeaddrinfo(res); // free the linked-list
    return 0;
}

//main method, server should take 6 arguments
//server port number, server port address, number of iterations, number of buffers, size of each buffer, and the type of operation to perform
int main(int argc, char* argv[]) {
    if (argc != 7) { //check that correct # of arguments were entered
        cerr << "Wrong number of arguments entered" << endl;
        exit(1);
    }
    if (stoi(argv[4]) * stoi(argv[5]) != 1500) { //check that resulting buffer size = 1500
        cerr << "nbufs * bufsize must equal 1500" << endl;
        exit(1);
    }
    return createConnection(argv[1], argv[2], stoi(argv[3]), stoi(argv[4]), stoi(argv[5]), stoi(argv[6]));
}


