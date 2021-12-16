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

#define MAXLINE 516
#define PORT 51948
#define RETRIES 10
#define TIMEOUT 2
#define HOST_ADDRESS "10.158.82.39"

//const int MAXLINE = 516, PORT = 51949, RETRIES = 10, TIMEOUT = 2;
//const char* HOST_ADDRESS = "10.158.82.39";

int sendFile(string filename, sockaddr_in recvaddr, int sockfd);

enum STEP {CLOSE, RETRY, WAIT, PROGRESS, START};

class Transaction {
public:
    sockaddr_in client;
    fstream file;
    int retries = 0, tid, len, sockfd;
    short curblock, lastack;
    timeval timeSent;
    STEP curStep = START;
};

class ReadRequest: public Transaction {
public:
    char buffer[MAXLINE];
    int curBlockSize = 512;
    long size;
    ReadRequest(sockaddr_in addr, int fd) {
        memset(&buffer, 0, MAXLINE);
        client = addr;
        tid = ntohs(client.sin_port);
        gettimeofday(&timeSent, NULL);
        len = sizeof(client);
        sockfd = fd;
        curblock = 0;
        lastack = 0;
    }
    virtual STEP nextStep() {
        timeval curtime;
        gettimeofday(&curtime, NULL);
        if (curStep == CLOSE) {
            return CLOSE;
        }
        else if (curStep == START) {
            return START;
        }
        else if (retries >= RETRIES) {
            cout << "too many retries" << endl;
            curStep = CLOSE;
            return curStep;
        }
        else if (curBlockSize < 512 && lastack == curblock) { //512 or maxline??
            cout << "last block transmitted & acked" << endl;
            curStep = CLOSE;
            return curStep;
        }
        else if (lastack == curblock) {
            curStep = PROGRESS;
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
    virtual bool start(string filename) {
        cout << "running start" << endl;
        file = fstream(filename, fstream::in | fstream::ate | fstream::binary);
        if (file.is_open() == false || !file.good()) {
            *((short*)buffer) = 5;
            *((short*)(buffer + 2)) = 1;
            const char* error = "Could not open file\0";
            cout << error << endl;
            strcpy(buffer + 4, error);
            sendto(sockfd, (const char*)buffer, 4 + sizeof(error), 0, (const struct sockaddr*)&client, len);
            curStep = CLOSE;
            return false;
        }
        long end = file.tellg();
        file.seekg(0, ios::beg);
        size = end - file.tellg();
        cout << "start finished" << endl;
        curStep = PROGRESS;
        return true;
    }
    virtual bool send() {
        cout << "RRQ send(): " << curStep << endl;
        if (curStep == PROGRESS) {
            curStep = WAIT;
            retries = 0;
            curblock++;
            cout << "progress" << endl;
            *((short*)buffer) = 3;
            *((short*)(buffer + 2)) = curblock;
            if (curblock * 512 > size) {
                curBlockSize = size - ((curblock - 1) * 512);
            }
            else {
                curBlockSize = 512;
            }
            cout << "reading file" << endl;
            file.read(buffer + 4, curBlockSize);
        }
        else {
            curStep = WAIT;
            retries++;
        }
        gettimeofday(&timeSent, NULL);
        cout << "sending block " << curblock << endl;
        int status = (int) sendto(sockfd, (const char*)buffer, 4 + curBlockSize, 0, (const struct sockaddr*)&client, len);
        if (status < 0) {
            cout << "sending error" << endl;
        }
        return true;
    }
    //recieves ack: must be 4 bytes
    virtual bool recieve(char* in) {
        cout << "recieving packet" << endl;
        if (*((short*)(in + 2)) == curblock) {
            lastack = *((short*)(in + 2));
            cout << curblock << " block acked" << endl;
        }
        else if (*((short*)(in + 2)) != curblock) {
            cout << "wrong ack recieved: " << lastack << endl;
            return false;
        }
        return true;
    }
};

class WriteRequest: public Transaction {
public:
    sockaddr_in client;
    fstream file;
    int retries = 0, tid, len, sockfd;
    short curblock = 0, lastack = 0;
    timeval timeSent;
    STEP curStep = START;
    WriteRequest(sockaddr_in addr, int fd) {
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
        else if (curStep == START) {
            return START;
        }
        else if (retries >= RETRIES) {
            curStep = CLOSE;
            return curStep;
        }
        else if (lastack < curblock) {
            curStep = PROGRESS;
            return PROGRESS;
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
    virtual bool start(string filename) {
        file = fstream(filename, fstream::out | fstream::binary | fstream::trunc);
        if (file.is_open() == false) {
            char buffer[MAXLINE];
            *((short*)buffer) = 5;
            *((short*)(buffer + 2)) = 1;
            const char* error = "Could not open file\0";
            cout << error << endl;
            strcpy(buffer + 4, error);
            sendto(sockfd, (const char*)buffer, 4 + sizeof(error), 0, (const struct sockaddr*)&client, len);
            curStep = CLOSE;
            return false;
        }
        /*else if (file.good()) {
            char buffer[MAXLINE];
            *((short*)buffer) = 5;
            *((short*)(buffer + 2)) = 6;
            const char* error = "File already exists\0";
            cout << error << endl;
            strcpy(buffer + 4, error);
            sendto(sockfd, (const char*)buffer, 4 + sizeof(error), 0, (const struct sockaddr*)&client, len);
            curStep = CLOSE;
            return false;
        }*/
        file.seekg(0, ios::beg);
        lastack = -1;
        curblock = 0;
        curStep = PROGRESS;
        return true;
    }
    virtual bool send() {
        if (curStep == PROGRESS) {
            lastack = curblock;
            retries = 0;
            curStep = WAIT;
        }
        cout << "WRQ send: ack " << lastack << endl;
        char ack[4];
        *((short*)ack) = 4;
        *((short*)(ack + 2)) = lastack;
        for (int i = 0; i < 4; i++) {
            cout << (int)ack[i];
        }
        cout << endl;
        int status = (int) sendto(sockfd, ack, 4, 0, (const struct sockaddr*)&client, len);
        if (status < 0) {
            cout << "sending error" << endl;
        }
        gettimeofday(&timeSent, NULL);
        retries++;
        return true;
    }
    virtual bool recieve(char* in, int nbytes) {
        char* readPtr = in + 2;
        if (*((short*)readPtr) == curblock + 1) {
            curblock = *((short*)readPtr);
            readPtr += 2;
            file.write(readPtr, nbytes - 4);
            if (nbytes < MAXLINE) {
                curStep = CLOSE;
            }
            return true;
        }
        return false;
    }
};