
//#pragma once
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
#include <signal.h>

#define MAXLINE 516
#define PORT 51948
#define RETRIES 10
#define TIMEOUT 2
#define HOST_ADDRESS "10.158.82.39"

using namespace std;

bool flag = true;

static void handler(int signum) {
    flag = false;
    signal(SIGALRM, handler);
}

//method for handling creating connection to server
//takes in port #, server address, number of iterations to run, number of buffers, size of buffers, and the type of sending method to use
int startTransfer(int port, const char* filename, const short opcode) {

    int sockfd;
    char buffer[MAXLINE];
    struct sockaddr_in server;
    signal(SIGALRM, handler);
    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    const int yes = 0;

    memset(&server, 0, sizeof(server));

    // Filling server information
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(HOST_ADDRESS);

    //struct timeval timeSent;
    //timeSent.tv_sec = TIMEOUT; /* seconds */
    //timeSent.tv_usec = 0;


    int len = sizeof(server);

    //set initial request buffer
    char* ptr = buffer;
    *((short*)ptr) = htons(opcode);
    ptr += 2;
    strcpy(ptr, filename);
    ptr += strlen(filename) + 1;
    strcpy(ptr, "octet\0");
    ptr += strlen("octet\0") + 1;

    //read requests
    if (opcode == 1) {
        cout << "RRQ " << filename << endl;
        sendto(sockfd, buffer, ptr - buffer, 0, (const struct sockaddr*)&server, len); //send request
        ofstream file(filename, ios::binary | std::ofstream::trunc); //open file
        file.seekp(0, ios::beg); //start at front of file
        int data = 0; //track size of data block
        bool transAcked = false; //whether initial transaction request was acknoledged
        bool recievedData = false; //whether recieved data this round
        short curblock = 1;
        //loop until last packet recieved, error occured, or retries excceded
        do {
            recievedData = false;
            for (int i = 0; i < RETRIES && !recievedData; i++) {
                alarm(TIMEOUT);
                int bytesRead = 0;
                flag = true;
                while (bytesRead <= 0 && flag) { //attempt to recieve data until timeout
                    bytesRead = recvfrom(sockfd, buffer, MAXLINE, MSG_DONTWAIT, (struct sockaddr*)&server, (socklen_t*)&len);
                }
                alarm(0);
                if (bytesRead <= 0) { //didn't recieve data, need to resent
                    if (transAcked == false) { //if didn't recieve response to initial request, send it again
                        cout << "Timed out: resending RRQ" << endl;
                        ptr = buffer;
                        *((short*)ptr) = htons(opcode);
                        ptr += 2;
                        strcpy(ptr, filename);
                        ptr += sizeof(filename);
                        *ptr = 0;
                        ptr++;
                        strcpy(ptr, "octet");
                        ptr += sizeof("octet");
                        *ptr = 0;
                        sendto(sockfd, buffer, ptr - buffer, 0, (const struct sockaddr*)&server, len);
                    }
                    else { //if didn't recieve response, send ACK again
                        cout << "Timed out: resending ACK " << curblock << endl;
                        char ack[4];
                        *((short*)ack) = htons(4);
                        *((short*)(ack + 2)) = htons(curblock);
                        sendto(sockfd, ack, 4, 0, (const struct sockaddr*)&server, len);
                    }
                }
                else if (ntohs(*((short*)buffer)) == 5) { //print and exit any errors
                    cout << "error " << ntohs(*((short*)(buffer + 2))) << ": " << string(buffer + 4) << endl;
                    exit(1);
                }
                else if (ntohs(*((short*)buffer)) == 3) { //recieved DATA
                    cout << "Recieved data " << ntohs(*((short*)(buffer + 2))) << endl;
                    transAcked = true;
                    //if recieved new block update curblock and write data, otherwise retransmit last ack
                    if (ntohs(*((short*)(buffer + 2))) > curblock) {
                        curblock = ntohs(*((short*)(buffer + 2)));
                        recievedData = true;
                        file.write(buffer + 4, bytesRead - 4);
                        data = bytesRead - 4;
                    }
                    cout << "Sending ACK " << curblock << endl;
                    char ack[4];
                    *((short*)ack) = htons(4);
                    *((short*)(ack + 2)) = htons(curblock);
                    sendto(sockfd, ack, 4, 0, (const struct sockaddr*)&server, len);
                }
            }
            if (recievedData == false) { //retry failed, exit
                cout << "retries failed, session ended" << endl;
                file.close();
                exit(0);
            }
        } while (data == 512);
        cout << "done" << endl;
        file.close();
    }
    //write request
    else if (opcode == 2) {
        cout << "WRQ :" << filename << endl;
        char ack[MAXLINE];
        int bytesRead = 0;
        int retries = 0;
        bool transAcked = false;
        while (retries < RETRIES && !transAcked) { //loop until write request is acknoledged or failed
            sendto(sockfd, buffer, ptr - buffer, 0, (const struct sockaddr*)&server, len); //send request, retrying each loop
            alarm(TIMEOUT);
            flag = true;
            while (bytesRead <= 0 && flag) { //attempt to recieve data until timeout
                bytesRead = (int)recvfrom(sockfd, ack, MAXLINE, MSG_DONTWAIT, (struct sockaddr*)&server, (socklen_t*)&len);
            }
            alarm(0);
            if (bytesRead >= 4) { //recieved packet
                cout << "packet recieved" << endl;
                retries = 0;
                if (ntohs(*((short*)ack)) == 5) { //recieved error, failed
                    cout << "recieved error " << ntohs(*((short*)(ack + 2))) << ": " << string(ack + 4) << endl;
                    return 0;
                }
                else if (ntohs(*((short*)ack)) == 4) { //recieved ack
                    retries = 0; //recieved ack, so reset retries;
                    if (ntohs(*((short*)(ack + 2))) == 0) { //recieved proper 0 ack
                        cout << "ack 0 recieved" << endl;
                            transAcked = true;
                    }
                }
            }
            if (!transAcked) {
                cout << "Timed out waiting for transaction ack, retrying..." << endl;
            }
            retries++;
        }
        if (retries == RETRIES || !transAcked) {
            cout << "Could not start transaction, closing" << endl;
            return 0;
        }

        ifstream file(filename, ios::ate | ios::binary); //open file
        long end = file.tellg();
        file.seekg(0, ios::beg);
        long size = end - file.tellg();
        char dataBuf[MAXLINE];
        int toRead, curblock = 1;
        do { //loop until last data block sent, error recieved, or retries exceeded
            *((short*)buffer) = htons(3);
            *((short*)(buffer + 2)) = htons(curblock);
            if (curblock * 512 > size) { //calculate size of next block
                toRead = size - ((curblock - 1) * 512);
            }
            else {
                toRead = 512;
            }
            file.read(buffer + 4, toRead); //read from file into buffer
            bool blockAcked = false;
            for (int i = 0; i < RETRIES && !blockAcked; i++) { //retry until ack for block recieved
                cout << "sending block " << curblock << " of data" << endl;
                int status = sendto(sockfd, (const char*)buffer, 4 + toRead, 0, (const struct sockaddr*)&server, len);
                alarm(TIMEOUT);
                int bytesRead = 0;
                flag = true;
                while (flag && bytesRead <= 0) { //try and recieve data until timeout
                    bytesRead = (int)recvfrom(sockfd, (char*)dataBuf, MAXLINE, MSG_DONTWAIT, (struct sockaddr*)&server, (socklen_t*)&len);
                }
                alarm(0);
                if (bytesRead >= 4) { //recieved data
                    if (ntohs(*((short*)dataBuf)) == 5) { //recieved error, fail
                        string error(dataBuf + 4);
                        cout << "Error " << ntohs(*((short*)(dataBuf + 2))) << ": " << error << endl;
                        file.close();
                        return 0;
                    }
                    else if (ntohs(*((short*)dataBuf)) == 4) { //recieved ack
                        cout << "Recieved ACK " << ntohs(*((short*)(dataBuf + 2))) << endl;
                        if (ntohs(*((short*)(dataBuf + 2))) == curblock) { //if ack for last data was recieved, update
                            blockAcked = true;
                            curblock++;
                            break;
                        }
                        //if recieved out of order ack, transmission is possible so don't time out
                        else if (ntohs(*((short*)(dataBuf + 2))) != curblock) {
                            i = 0;
                        }
                    }
                    else if (ntohs(*((short*)ack)) != 4) {
                        cout << "wrong packet type" << ntohs(*((short*)ack)) << endl;
                    }
                }
                if (!blockAcked) {
                    cout << "timed out waiting for ack, retrying..." << endl;
                }
            }
            if (!blockAcked) {
                cout << "retries failed, session ended" << endl;
                file.close();
                return 0;
            }
        } while (toRead == 512);
        file.close();
    }
    cout << "done" << endl;
    close(sockfd);
    return 0;
}

//main method, server should take 3 or 5 arguments
//-r/-w followed by file name, then optionally -p port
int main(int argc, char* argv[]) {
    int opcode = 0;
    const char* filename;
    int port = PORT;

    if (argc != 3 && argc != 5) {
        cout << "wrong number of arguments" << endl;
        exit(1);
    }
    string flag = argv[1];
    if (flag == "-r") { //read request
        opcode = 1;
        filename = argv[2];
    }
    else if (flag == "-w") { //write request
        opcode = 2;
        filename = argv[2];
        if (access(filename, F_OK) == -1) { //check if file exists first
            cout << "file does not exist" << endl;
            exit(1);
        }
    }
    else {
        cout << "first flag must be -r or -w" << endl;
        exit(1);
    }
    if (strchr(filename, '/') != NULL || strchr(filename,'\\') != NULL) {
        cout << "Tried to access file path" << endl;
        exit(1);
    }

    if (argc == 5 && string(argv[3]) == "-p") {
        port = stoi(argv[4]);
        if (port < 0) {
            cout << "bad port" << endl;
            exit(1);
        }
    }

    return startTransfer(port, filename, opcode);
    exit(0);
}

