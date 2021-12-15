#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>   // htonl, htons, inet_ntoa 
#include <arpa/inet.h>    // inet_ntoa 
#include <netdb.h>        // gethostbyname 
#include <unistd.h>       // read, write, close 
#include <sys/time.h>
#include <vector>


using namespace std;

const int MAXLINE = 516, PORT = 51949, RETRIES = 10, TIMEOUT = 2;
const char* HOST_ADDRESS = "10.158.82.39";

int sendFile(string filename, sockaddr_in recvaddr, int sockfd);

enum STEP {CLOSE, RETRY, WAIT, PROGRESS, START};

class Transaction {
public:
    sockaddr_in client;
    fstream file;
    int retries = 0, curblock = 0, lastack = 0, tid, len, sockfd;
    timeval timeSent;
    STEP curStep = START;
    char buffer[MAXLINE];
};

class ReadRequest: public Transaction {
public:
    ReadRequest(sockaddr_in addr, int fd) {
        memset(&buffer, 0, MAXLINE);
        client = addr;
        tid = ntohs(client.sin_port);
        gettimeofday(&timeSent, NULL);
        len = sizeof(client);
        sockfd = fd;
    }
    virtual STEP nextStep() {
        timeval curtime;
        gettimeofday(&curtime, NULL);
        if (curStep == CLOSE) {
            return CLOSE;
        }
        else if (retries >= RETRIES) {
            curStep = CLOSE;
            return curStep;
        }
        else if (curtime.tv_sec - timeSent.tv_sec >= TIMEOUT) {
            curStep = RETRY;
            return curStep;
        }
        else if (lastack == curblock) {
            curStep = PROGRESS;
            return curStep;
        }
        else {
            curStep = WAIT;
            return curStep;
        }
    }
    virtual bool start(string filename) {
        file = fstream(filename, fstream::in | fstream::ate | fstream::binary);
        if (file.is_open() == false) {
            *((short*)buffer) = 5;
            *((short*)(buffer + 2)) = 1;
            const char* error = "Could not open file\0";
            cout << error << endl;
            strcpy(buffer + 4, error);
            sendto(sockfd, (const char*)buffer, 4 + sizeof(error), 0, (const struct sockaddr*)&client, len);
            curStep = CLOSE;
        }
    }
};

class WriteRequest: public Transaction {
public:
    WriteRequest(string filename, sockaddr_in addr, int fd) {
        memset(&buffer, 0, MAXLINE);
        client = addr;
        tid = ntohs(client.sin_port);
        gettimeofday(&timeSent, NULL);
        file = fstream(filename, fstream::out | fstream::binary | fstream::trunc);
        len = sizeof(client);
        sockfd = fd;
    }
    virtual STEP nextStep() {
        timeval curtime;
        gettimeofday(&curtime, NULL);
        if (retries >= RETRIES) {
            curStep = CLOSE;
            return curStep;
        }
        else if (curtime.tv_sec - timeSent.tv_sec >= TIMEOUT) {
            curStep = RETRY;
            return curStep;
        }
        else {
            curStep = WAIT;
            return curStep;
        }
    }
};