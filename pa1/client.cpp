#include <iostream>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/time.h>


using namespace std;

int createConnection(const char* port, const char* address, int iterations, int nbufs, int bufsize, int type) {
    //const int iterations = (int)argv[2], nbufs = (int)argv[3], bufsize = (int)argv[4], type = (int)argv[5];

    struct addrinfo hints;
    struct addrinfo* res;
    int status;

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets

    
    if ((status = getaddrinfo(address, port, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return 1;
    }

    int sd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    if (connect(sd, res->ai_addr, res->ai_addrlen) == -1) {
        cerr << "Connection error";
        return 1;
    }
    cout << "iterations = " << iterations << ", nbufs =" << nbufs << ", bufsize = " 
        << bufsize << " type = " << type << endl;
    struct timeval start_time, lap_time, end_time;
    int nReads;
    char** databuf = new char* [nbufs];
    for (int i = 0; i < nbufs; i++) {
        databuf[i] = new char[bufsize];
    }

    switch (type) {
    case 1: {
        gettimeofday(&start_time, NULL);
        for (int i = 0; i < iterations; i++)
            for (int j = 0; j < nbufs; j++)
                write(sd, databuf[j], bufsize); // sd: socket descriptor
        gettimeofday(&lap_time, NULL);
        read(sd, &nReads, sizeof(int));
        gettimeofday(&end_time, NULL);
        cout << "Test 1: ";
        break;
    }
    case 2: {
        gettimeofday(&start_time, NULL);
        for (int i = 0; i < iterations; i++) {
            iovec* vector = new iovec[nbufs];
            for (int j = 0; j < nbufs; j++) {
                vector[j].iov_base = databuf[j];
                vector[j].iov_len = bufsize;
            }
            writev(sd, vector, nbufs); // sd: socket descriptor
        }
        gettimeofday(&lap_time, NULL);      
        read(sd, &nReads, sizeof(int));
        gettimeofday(&end_time, NULL);
        cout << "Test 2: ";
        break;
    }
    case 3: {
        //char* databuf = new char[nbufs * bufsize];
        gettimeofday(&start_time, NULL);
        for (int i = 0; i < iterations; i++) {
            write(sd, databuf, nbufs * bufsize); // sd: socket descriptor
        }
        gettimeofday(&lap_time, NULL);    
        read(sd, &nReads, sizeof(int));
        gettimeofday(&end_time, NULL);
        cout << "Test 3: ";
        break;
    }
    default:
        cout << "Bad type selection" << endl;
    }
    
    cout << "data receiving time = " << lap_time.tv_usec - start_time.tv_usec << " usec, ";
    cout << "round trip time = " << end_time.tv_usec - lap_time.tv_usec << " usec, ";
    cout << "#reads = " << nReads << " times" << endl;

    for (int i = 0; i < nbufs; i++) {
        delete[] databuf[i];
    }
    delete[] databuf;
    freeaddrinfo(res); // free the linked-list
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 7) {
        cerr << "Wrong number of arguments entered" << endl;
        exit(1);
    }
    if (stoi(argv[4]) * stoi(argv[5]) != 1500) {
        cerr << "nbufs * bufsize must equal 1500" << endl;
        exit(1);
    }
    createConnection(argv[1], argv[2], stoi(argv[3]), stoi(argv[4]), stoi(argv[5]), stoi(argv[6]));
    return 0;
}


