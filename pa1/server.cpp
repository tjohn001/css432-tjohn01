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
const int BUFSIZE = 1500;

void *recieve_data(void* ptr) {
    int* args = (int*)ptr;
    int databuf[BUFSIZE];
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    int count = 0;
    for (int i = 0; i < args[1]; i++) {
        for (int nRead = 0;
            (nRead += read(args[0], databuf, BUFSIZE - nRead)) < BUFSIZE;
            ++count);
    }
    close(args[1]);
    return;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Wrong number of arguments entered" << endl;
        return 1;
    }

    struct addrinfo hints;
    struct addrinfo* res;
    int status;

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me


    if ((status = getaddrinfo(NULL, argv[0], &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return 1;
    }

    int sd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    const int yes = 0;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes));

    bind(sd, res->ai_addr, res->ai_addrlen);
    listen(sd, 20);

    while (true) {
        struct sockaddr_storage newSockAddr;
        socklen_t newSockAddrSize = sizeof(newSockAddr);
        int newSd = accept(sd, (struct sockaddr*)&newSockAddr, &newSockAddrSize);
        int args[] = { newSd, stoi(argv[2]) };
        pthread_t thread;
        int iret = pthread_create(&thread, NULL, recieve_data, (void*)args);
        //pthread_join(thread, NULL);
    }
    close(sd);
    return 0;
}