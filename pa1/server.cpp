/*
* Taylor Johnston
* Server side application that recieves a given iterations of 1500 bytes on a given port
* Outputs the time taken to recieve the data
*/

#include <iostream>
#include <cstring>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/time.h>
#include <netinet/in.h>   // htonl, htons, inet_ntoa 
#include <arpa/inet.h>    // inet_ntoa 
#include <netdb.h>        // gethostbyname 
#include <unistd.h>       // read, write, close 
#include <strings.h>      // bzero 
#include <netinet/tcp.h>  // SO_REUSEADDR 
#include <pthread.h>


using namespace std;
const int BUFSIZE = 1500; //size of buffer to recieve

//thread to read data, takes a pointer to a 2 element int array
//ptr[0] is the file descriptor of the socket to read from
//ptr[1] is the number of iterations
void *recieve_data(void* ptr) {
    int* args = (int*)ptr; //cast paramaters back to int array
    int databuf[BUFSIZE]; //allocate array to hold buffer
    struct timeval start_time, end_time; //timer
    gettimeofday(&start_time, NULL); //start timer
    int count = 0;
    int readVal = 0;
    for (int i = 0; i < args[1]; i++) {
        for (int nRead = 0;
            (nRead += read(args[0], databuf, BUFSIZE - nRead)) < BUFSIZE; //read bytesfrom fd into databuf up to BUFFSIZE
            ++count);
    }
    gettimeofday(&end_time, NULL); //stop timer
    cout << "data receiving time = " << (end_time.tv_sec * 1e6 + end_time.tv_usec) - (start_time.tv_sec * 1e6 + start_time.tv_usec) << "usec" << endl;
    write(args[0], &count, sizeof(int)); //send number of writes made
    close(args[0]); //close socket fd
}

//main method, server should take 2 args - the port number and the number of iterations
int main(int argc, char* argv[]) {
    if (argc != 3) { //fail if given wrong number of args
        cerr << "Wrong number of arguments entered" << endl;
        return 1;
    }

    //setup server socket
    struct addrinfo hints; 
    struct addrinfo* res;
    int status;

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    //add server address info to res
    if ((status = getaddrinfo(NULL, argv[1], &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return 1;
    }

    //socket file descriptor
    int sd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    const int yes = 0;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes)); //setup socket

    bind(sd, res->ai_addr, res->ai_addrlen); //bind socket
    listen(sd, 20); //listen on socket

    //accept requests with new thread
    while (true) {
        struct sockaddr_storage newSockAddr;
        socklen_t newSockAddrSize = sizeof(newSockAddr);
        int newSd = accept(sd, (struct sockaddr*)&newSockAddr, &newSockAddrSize); //fd of new socket
        int args[] = { newSd, stoi(argv[2]) };
        pthread_t thread;
        int iret = pthread_create(&thread, NULL, &recieve_data, (void*)args); //create new recieve_data thread
        //pthread_join(thread, NULL);
    }
    close(sd); //close socket fd
    return 0;
}