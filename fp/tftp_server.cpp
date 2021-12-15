#pragma once
#include <iostream>
#include <cstring>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <netinet/in.h>   // htonl, htons, inet_ntoa 
#include <arpa/inet.h>    // inet_ntoa 
#include <netdb.h>        // gethostbyname 
#include <unistd.h>       // read, write, close 
#include <strings.h>      // bzero 
#include <netinet/tcp.h>  // SO_REUSEADDR 
#include <pthread.h>
#include <fstream>
#include <list>
#include "tftp.h"

using namespace std;

//thread to read data, takes a pointer to a 2 element int array
//ptr[0] is the file descriptor of the socket to use
//ptr[1] is addrinfo* ptr to send datagram through
/*void* recieve_data(void* ptr) {
    int* args = (int*)ptr; //cast paramaters back to int array
    int databuf[BUFSIZE]; //allocate array to hold buffer
    for (int i = 0; i < args[1]; i++) {
        for (int nRead = 0;
            (nRead += read(args[0], databuf, BUFSIZE - nRead)) < BUFSIZE; //read bytesfrom fd into databuf up to BUFFSIZE
            ++count);
    }
    write(args[0], &count, sizeof(int)); //send number of writes made
    close(args[0]); //close socket fd

    int sockfd = *(int*)ptr;
    struct addrinfo* recvaddr = (addrinfo*)(ptr + sizeof(int));
    char buffer[MAXLINE];
    recvfrom()

}*/

class Transfer {
public:
    int tid, filePosition = 0, sockfd, size, opmode, clientlen;
    fstream file;
    short block = 0;
    sockaddr_in client;
    list<Transfer> *list;
};

//int read(Transfer* transfer) {
int sendFile(string filename, sockaddr_in recvaddr, int sockfd) {
    int tid = ntohs(recvaddr.sin_port);
    ifstream file(filename);
    int end = file.tellg();
    file.seekg(0, ios::beg);
    int size = end - file.tellg();
    struct sockaddr_in data;
    memset(&data, 0, sizeof(data));
    int len = sizeof(recvaddr);
    int dataLen = sizeof(data);
    char buffer[MAXLINE];
    char dataBuf[MAXLINE];
    int count, block = 1;
    do {
        *((short*)buffer) = 3;
        *((short*)(buffer + 2)) = block;
        int toRead;
        if (block * 512 > size) {
            toRead = size - ((block - 1) * 512);
        }
        else {
            toRead = 512;
        }
        cout << "reading file" << endl;
        file.read(buffer + 4, toRead);
        struct timeval start_time, cur_time; //timer
        bool blockAcked = false;
        for (int i = 0; i < RETRIES && !blockAcked; i++) {
            cout << "sending bytes: " << toRead << "in block" << block << endl;
            int status = sendto(sockfd, (const char*)buffer, 4 + toRead, 0, (const struct sockaddr*)&recvaddr, len);
            gettimeofday(&start_time, NULL); //start timer
            gettimeofday(&cur_time, NULL);
            while (cur_time.tv_sec - start_time.tv_sec < TIMEOUT && !blockAcked) {
                int bytesRead = recvfrom(sockfd, (char*)dataBuf, MAXLINE, MSG_WAITALL, (struct sockaddr*)&data, (socklen_t*)&dataLen);
                if (bytesRead >= 4) {
                    if (tid == ntohs(data.sin_port)) {
                        if (*((short*)dataBuf) == 5) {
                            string error(dataBuf + 4);
                            cout << "Error " << *((short*)(dataBuf + 2)) << error << endl;
                            return;
                        }
                        else if (*((short*)dataBuf) == 4) {
                            if (*((short*)(dataBuf + 2)) == block) {
                                cout << "block acked" << endl;
                                blockAcked = true;
                                block++;
                                break;
                            }
                            else if (*((short*)(dataBuf + 2)) != block) {
                                cout << "wrong ack recieved" << endl;
                            }
                        }
                    }
                }
                gettimeofday(&cur_time, NULL);
            }
            if (!blockAcked) {
                cout << "timed out waiting for ack, retrying..." << endl;
            }
        }
        if (!blockAcked) {
            cout << "retries failed, session ended" << endl;
            file.close();
            return -1;
        }
    } while (count == 512);
    file.close();
    return 0;
}

//main method, server should take 2 args - the port number and the number of iterations
int main(int argc, char* argv[]) {
    /*if (argc != 3) { //fail if given wrong number of args
        cerr << "Wrong number of arguments entered" << endl;
        return 1;
    }*/

    //setup server socket
    int sockfd;
    char buffer[MAXLINE];
    struct sockaddr_in server, client;
    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server, 0, sizeof(server));
    memset(&client, 0, sizeof(client));

    // Filling server information
    server.sin_family = AF_INET; // IPv4
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    // Bind the socket with the server address
    if (bind(sockfd, (const struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    timeval tv;
    tv.tv_sec = TIMEOUT; /* seconds */
    tv.tv_usec = 0;
    //if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv) < 0))
        cout << "Cannot Set SO_SNDTIMEO for socket" << endl;
    int len;

    len = sizeof(client); //len is value/resuslt
    
    int bytesRead = recvfrom(sockfd, (char*)buffer, MAXLINE, MSG_WAITALL, (struct sockaddr*)&client, (socklen_t*)&len);
    //TODO: add check that ptr < bytes read
    char* ptr = buffer;
    short opcode = *((short*)ptr);
    if (opcode < 1 || opcode > 7) {
        *((short*)buffer) = 5;
        *((short*)(buffer + 2)) = 4;
        char* error = "Uknown operation\0";
        cout << error << endl;
        strcpy(buffer + 4, error);
        sendto(sockfd, (const char*)buffer, 4 + sizeof(error), 0, (const struct sockaddr*)&client, len);
    }
    ptr += 2;
    string filename(ptr);
    ptr += filename.length() + 1;
    string mode(ptr);

    cout << endl;

    cout << "opcode: " << opcode << ", filename: " << filename << ", mode: " << mode << endl;

    if (filename.find('/') != -1 || filename.find('\\') != -1) {
        *((short*)buffer) = 5;
        *((short*)(buffer + 2)) = 2;
        char* error = "Attempted to access file path\0";
        strcpy(buffer + 4, error);
        sendto(sockfd, (const char*)buffer, 4 + sizeof(error), 0, (const struct sockaddr*)&client, len);
    }
    else if (opcode == 1) {
        sendFile(filename, client, sockfd);
        /*cout << "RRQ :" << filename << endl;
        ifstream file(filename, ios::ate | ios::binary);
        cout << "file opened" << endl;
        int end = file.tellg();
        file.seekg(0, ios::beg);
        int size = end - file.tellg();
        //ptr = buffer;
        char ack[4];
        cout << "start: " << file.tellg() << " end: " << end << " size: " << size;
        for (int block = 1; file.tellg() <= end; block++) {
            *((short*)buffer) = 3;
            *((short*)(buffer + 2)) = block;
            int toRead;
            if (block * 512 > size)
                toRead = size - ((block - 1)* 512);
            else
                toRead = 512;
            cout << "reading file" << endl;
            file.read(buffer + 4, toRead);
            cout << "sending bytes: " << toRead << endl;
            cout << "bytes sent: " << (int) sendto(sockfd, (const char*) buffer, 4 + toRead, MSG_CONFIRM, (const struct sockaddr*)&client, len) << endl;
            cout << "block sent: " << *((short*)(buffer + 2)) << endl;
            bytesRead = recvfrom(sockfd, ack, 4, MSG_WAITALL, (struct sockaddr*)&client, (socklen_t*)&len);
            cout << "ack recieved" << endl;
            if (bytesRead != 4) {
                cout << "packet error" << endl;
            }
            else if (*((short*)ack) != 4) {
                cout << "wrong packet type" << *((short*)ack) << endl;
            }
            else if (*((short*)(ack + 2)) != block) {
                cout << "wrong ack #: " << block << " vs " << *((short*)(ack + 2)) << endl;
            }
        }
        file.close();*/
    }
    else if (opcode == 2) {
        cout << "WRQ " << filename << endl;
        char ack[4];
        *((short*)ack) = 4;
        *((short*)(ack + 2)) = 0;
        sendto(sockfd, ack, 4, MSG_CONFIRM, (const struct sockaddr*)&client, len);
        ofstream file(filename, ios::binary | std::ofstream::trunc);
        file.seekp(0, ios::beg);
        int data = 512;
        do {
            cout << "wait for packet" << endl;
            int bytesRead = recvfrom(sockfd, buffer, MAXLINE, MSG_WAITALL, (struct sockaddr*)&client, (socklen_t*)&len);
            cout << "recieved bytes: " << bytesRead << endl;
            if (bytesRead < 2) {
                cout << "read error";
            }
            else if (*((short*)buffer) == 5) {
                cout << "error";
                break;
            }
            else if (*((short*)buffer) != 3) {
                cout << "wrong packet type: " << *((short*)buffer);
            }
            else {
                char* readPtr = buffer + 2;
                short block = *((short*)readPtr);
                readPtr += 2;
                file.write(readPtr, bytesRead - 4);
                data = bytesRead - 4;

                char* tempPtr = ack;
                *((short*)ack) = 4;
                *((short*)(ack + 2)) = block;
                cout << "read " << bytesRead << " send ack " << block << endl;
                sendto(sockfd, ack, 4, MSG_CONFIRM, (const struct sockaddr*)&client, len);
                /* resend ack on timeout*/
            }
        } while (data == 512);
        cout << "done" << endl;
        file.close();
    }




    //accept requests with new thread
    /*while (true) {
        struct sockaddr_storage newSockAddr;
        socklen_t newSockAddrSize = sizeof(newSockAddr);
        int newSd = accept(sockfd, (struct sockaddr*)&newSockAddr, &newSockAddrSize); //fd of new socket
        int args[] = { newSd, stoi(argv[2]) };
        pthread_t thread;
        int iret = pthread_create(&thread, NULL, &recieve_data, (void*)args); //create new recieve_data thread
        //pthread_join(thread, NULL);
    }*/
    close(sockfd); //close socket fd
    return 0;
}