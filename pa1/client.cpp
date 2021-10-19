#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/uio.h>


using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 6) {
        cerr << "Wrong number of arguments entered" << endl;
        return 1;
    }

    
}

int createConnection(char* argv) {
    const int iterations = (int)argv[2], nbufs = (int)argv[3], bufsize = (int)argv[4], type = (int)argv[5];

    struct addrinfo hints;
    struct addrinfo* servinfo;
    int status;

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    
    if ((status = getaddrinfo(argv[1], argv[0], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return 1;
    }

    int sd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

    if (connect(sd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        cerr << "Connection error";
        return 1;
    }

    if (nbufs * bufsize != 1500) {
        cerr << "nbufs * bufsize must equal 1500" << endl;
        return 1;
    }
    char** databuf = new char* [nbufs];
    for (int i = 0; i < nbufs; i++) {
        databuf[i] = new char[bufsize];
    }

    switch ((int)argv[5]) {
    case 1:
        for (int i = 0; i < iterations; i++)
            for (int j = 0; j < nbufs; j++)
                write(sd, databuf[j], bufsize); // sd: socket descriptor
        break;
    case 2:
        /*for (int i = 0; i < iterations; i++) {
            iovec vector[nbufs];
            for (int j = 0; j < nbufs; j++) {
                vector[j].iov_base = databuf[j];
                vector[j].iov_len = bufsize;
            }
            writev(sd, vector, nbufs); // sd: socket descriptor
        }*/
        break;
    case 3:
        for (int i = 0; i < iterations; i++) {
            write(sd, databuf, nbufs * bufsize); // sd: socket descriptor
        }
        break;
    default:
        cout << "Bad type selection" << endl;
    }

    freeaddrinfo(servinfo); // free the linked-list
    for (int i = 0; i < nbufs; i++) {
        delete[] databuf[i];
    }
    delete[] databuf;
}


