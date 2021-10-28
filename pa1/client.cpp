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
int createConnection(const char* port, const char* address, int iterations, int nbufs, int bufsize, int type) {

    //setup server socket
    struct addrinfo hints;
    struct addrinfo* res;
    int status;

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets

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
    cout << "iterations = " << iterations << ", nbufs =" << nbufs << ", bufsize = " 
        << bufsize << " type = " << type << endl; //output run conditions
    struct timeval start_time, lap_time, end_time;
    int nReads;
    //initialize new databuffer of size [nbufs][bufsize]
    char** databuf = new char* [nbufs];
    for (int i = 0; i < nbufs; i++) {
        databuf[i] = new char[bufsize];
    }
    //select between types of write methods
    switch (type) {
    case 1: { //send each buffer one at a time
        gettimeofday(&start_time, NULL); //start timer
        for (int i = 0; i < iterations; i++)
            for (int j = 0; j < nbufs; j++)
                write(sd, databuf[j], bufsize); // sd: socket descriptor
        gettimeofday(&lap_time, NULL); //lap timer, get data recieving time
        read(sd, &nReads, sizeof(int)); //read number of reads from sd
        gettimeofday(&end_time, NULL); //stop timer, get RTT
        cout << "Test 1: ";
        break;
    }
    case 2: { //send each buffer using an iovec
        gettimeofday(&start_time, NULL); //start timer
        for (int i = 0; i < iterations; i++) {
            iovec* vector = new iovec[nbufs];
            for (int j = 0; j < nbufs; j++) {
                vector[j].iov_base = databuf[j];
                vector[j].iov_len = bufsize;
            }
            writev(sd, vector, nbufs); // sd: socket descriptor
        }
        gettimeofday(&lap_time, NULL); //lap timer, get data recieving time
        read(sd, &nReads, sizeof(int)); //read number of reads from sd
        gettimeofday(&end_time, NULL); //stop timer, get RTT
        cout << "Test 2: ";
        break;
    }
    case 3: { //send buffers all at once
        //char* databuf = new char[nbufs * bufsize];
        gettimeofday(&start_time, NULL);
        for (int i = 0; i < iterations; i++) {
            write(sd, databuf, nbufs * bufsize); // sd: socket descriptor
        }
        gettimeofday(&lap_time, NULL); //lap timer, get data recieving time
        read(sd, &nReads, sizeof(int)); //read number of reads from sd
        gettimeofday(&end_time, NULL); //stop timer, get RTT
        cout << "Test 3: ";
        break;
    }
    default:
        cout << "Bad type selection" << endl;
    }
    //output transmission stats
    cout << "data receiving time = " << (lap_time.tv_sec * 1e6 + lap_time.tv_usec) - (start_time.tv_sec * 1e6 + start_time.tv_usec) << " usec, ";
    cout << "round trip time = " << (end_time.tv_sec * 1e6 + end_time.tv_usec) - (lap_time.tv_sec * 1e6 + lap_time.tv_usec) << " usec, ";
    cout << "#reads = " << nReads << " times" << endl;
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


