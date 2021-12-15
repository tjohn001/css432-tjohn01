
//#pragma once
#include "tftp.h"

using namespace std;

//method for handling creating connection to server
//takes in port #, server address, number of iterations to run, number of buffers, size of buffers, and the type of sending method to use
int startTransfer(const char* port, const char* filename, const short opcode) {

    int sockfd;
    char buffer[MAXLINE];
    struct sockaddr_in server;

    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server, 0, sizeof(server));

    // Filling server information
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr(HOST_ADDRESS);

    timeval timeSent;
    timeSent.tv_sec = TIMEOUT; /* seconds */
    timeSent.tv_usec = 0;

    if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeSent, sizeof(timeSent) < 0))
        cout << "Cannot Set SO_SNDTIMEO for socket" << endl;

    int len = sizeof(server);

    char* ptr = buffer;
    *((short*)ptr) = opcode;
    ptr += 2;
    strcpy(ptr, filename);
    ptr += sizeof(filename);
    *ptr = 0;
    ptr++;
    strcpy(ptr, "octet");
    ptr += sizeof("octet");
    *ptr = 0;
    
    sendto(sockfd, buffer, ptr - buffer, MSG_CONFIRM, (const struct sockaddr*)&server, len);

    if (opcode == 1) {
        cout << "RRQ " << filename << endl;
        ofstream file (filename, ios::binary | std::ofstream::trunc);
        file.seekp(0, ios::beg);
        int data = 512;
        short curblock;
        do {
            cout << "wait for packet" << endl;
            int bytesRead = recvfrom(sockfd, buffer, MAXLINE, MSG_WAITALL, (struct sockaddr*)&server, (socklen_t*)&len);
            cout << "recieved bytes: " << bytesRead << endl;
            if (bytesRead < 2) {
                cout << "read error";
            }
            else if (*((short*)buffer) == 5) {
                cout << "error";
                return -1;
            }
            else if (*((short*)buffer) != 3) {
                cout << "wrong packet type: " << *((short*)buffer);
            }
            else {
                char* readPtr = buffer + 2;
                short curblock = *((short*)readPtr);
                readPtr += 2;
                file.write(readPtr, bytesRead - 4);
                data = bytesRead - 4;
                
                char ack[4];
                char* tempPtr = ack;
                *((short*)ack) = 4;
                *((short*)(ack + 2)) = curblock;
                cout << "read " << bytesRead << " send ack " << curblock << endl;
                sendto(sockfd, ack, 4, 0, (const struct sockaddr*)&server, len);
                /* resend ack on timeout*/
            }
        } while (data == 512);
        cout << "done" << endl;
        file.close();
    }
    else if (opcode == 2) {
        cout << "WRQ :" << filename << endl;
        sendFile(filename, server, sockfd);
    }
    close(sockfd);
    return 0;
}

//main method, server should take 6 arguments
//server port number, server port address, number of iterations, number of buffers, size of each buffer, and the type of operation to perform
int main(int argc, char* argv[]) {
    /*if (argc != 7) { //check that correct # of arguments were entered
        cerr << "Wrong number of arguments entered" << endl;
        exit(1);
    }
    if (stoi(argv[4]) * stoi(argv[5]) != 1500) { //check that resulting buffer size = 1500
        cerr << "nbufs * bufsize must equal 1500" << endl;
        exit(1);
    }*/

    return startTransfer("54948", "test.txt", 1);

    //return startTransfer(argv[1], argv[2], stoi(argv[3]), stoi(argv[4]), stoi(argv[5]), stoi(argv[6]));
}


