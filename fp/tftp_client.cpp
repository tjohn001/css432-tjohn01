
//#pragma once
#include "tftp.h"
#include <signal.h>

using namespace std;

//handler from alarm - doesn't need to do anything
static void handler(int signum) {
    cout << "Handling timeout" << endl;
    alarm(0);
    //signal(SIGALRM, handler);
}


//method for handling creating connection to server
//takes in port #, server address, number of iterations to run, number of buffers, size of buffers, and the type of sending method to use
int startTransfer(int port, const char* filename, const short opcode) {

    int sockfd;
    char buffer[MAXLINE];
    struct sockaddr_in server;
    //signal(SIGALRM, handler);
    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    const int yes = 0;

    memset(&server, 0, sizeof(server));

    // Filling server information
    server.sin_family = AF_INET;
    server.sin_port = htonl(PORT);
    server.sin_addr.s_addr = inet_addr(HOST_ADDRESS);

    /*struct timeval timeSent;
    timeSent.tv_sec = TIMEOUT;
    timeSent.tv_usec = 0;*/

    int len = sizeof(server);

    char* ptr = buffer;
    *((short*)ptr) = htons(opcode);
    ptr += 2;
    strcpy(ptr, filename);
    ptr += sizeof(filename);
    *ptr = 0;
    ptr++;
    strcpy(ptr, "octet");
    ptr += sizeof("octet");
    *ptr = 0;

    if (opcode == 1) {
        cout << "RRQ " << filename << endl;
        int status = sendto(sockfd, buffer, ptr - buffer, 0, (const struct sockaddr*)&server, len);
        if (status < 0) {
            int error_code;
            int error_code_size = sizeof(error_code);
            getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error_code, (socklen_t*)&error_code_size);
            cout << "socket error: " << strerror(error_code);
            cout << "send error: " << strerror(errno) << endl;
            cout << "ptr - buffer: " << (ptr - buffer) << endl;
        }
        ofstream file(filename, ios::binary | std::ofstream::trunc);
        file.seekp(0, ios::beg);
        int data = 0;
        bool transAcked = false;
        bool recievedData = false;
        short curblock = 1;
        do {
            recievedData = false;
            for (int i = 0; i < RETRIES && !recievedData; i++) {
                cout << "trying to recieve data" << endl;
                //alarm(TIMEOUT);
                int bytesRead = recvfrom(sockfd, buffer, MAXLINE, MSG_DONTWAIT, (struct sockaddr*)&server, (socklen_t*)&len);

                if (bytesRead < 0) {
                    cout << "recv error: " << strerror(errno);
                }
                cout << "read bytes " << bytesRead << endl;
                //alarm(0);
                if (bytesRead == 0) {
                    if (transAcked == false) { //if didn't recieve response to request, send it again
                        cout << "Retrying RRQ" << endl;
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
                else if (ntohs(*((short*)buffer)) == 5) {
                    cout << "error " << ntohs(*((short*)(buffer + 2))) << ": " << string(buffer + 4) << endl;
                    exit(1);
                }
                else if (ntohs(*((short*)buffer)) == 3) {
                    transAcked = true;
                    //if recieved new block update curblock and write data, otherwise retransmit last ack
                    if (ntohs(*((short*)(buffer + 2))) > curblock) {
                        curblock = ntohs(*((short*)(buffer + 2)));
                        recievedData = true;
                        file.write(buffer + 4, bytesRead - 4);
                        data = bytesRead - 4;
                    }
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
    else if (opcode == 2) {
        cout << "WRQ :" << filename << endl;
        char ack[MAXLINE];
        int bytesRead = 0;
        int retries = 0;
        bool transAcked = false;
        while (retries < RETRIES && !transAcked) {
            cout << "Sending WRQ: " << filename << endl;
            sendto(sockfd, buffer, ptr - buffer, 0, (const struct sockaddr*)&server, len);
            //alarm(TIMEOUT);
            bytesRead = (int)recvfrom(sockfd, ack, MAXLINE, MSG_WAITALL, (struct sockaddr*)&server, (socklen_t*)&len);
            cout << "bytes read " << bytesRead << endl;
            //alarm(0);
            if (bytesRead >= 4) {
                cout << "packet recieved" << endl;
                retries = 0;
                if (ntohs(*((short*)ack)) == 5) {
                    cout << "recieved error" << endl;
                    return 0;
                }
                else if (ntohs(*((short*)(ack + 2)) && ntohs(*((short*)ack)) != 4) == 0) {
                    cout << "ack 0 recieved" << endl;
                    transAcked = true;
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

        ifstream file(filename, ios::ate | ios::binary);
        long end = file.tellg();
        file.seekg(0, ios::beg);
        long size = end - file.tellg();
        char dataBuf[MAXLINE];
        int toRead, curblock = 1;
        do {
            *((short*)buffer) = htons(3);
            *((short*)(buffer + 2)) = htons(curblock);
            if (curblock * 512 > size) {
                toRead = size - ((curblock - 1) * 512);
            }
            else {
                toRead = 512;
            }
            file.read(buffer + 4, toRead);
            bool blockAcked = false;
            for (int i = 0; i < RETRIES && !blockAcked; i++) {
                cout << "sending bytes: " << toRead << "in block" << curblock << endl;
                int status = sendto(sockfd, (const char*)buffer, 4 + toRead, 0, (const struct sockaddr*)&server, len);
                //alarm(TIMEOUT);
                int bytesRead = (int)recvfrom(sockfd, (char*)dataBuf, MAXLINE, MSG_WAITALL, (struct sockaddr*)&server, (socklen_t*)&len);
                //alarm(0);
                if (bytesRead < 0) {
                    cout << "recv error: " << strerror(errno) << endl;
                }
                if (bytesRead >= 4) {
                    if (ntohs(*((short*)dataBuf)) == 5) {
                        string error(dataBuf + 4);
                        cout << "Error " << ntohs(*((short*)(dataBuf + 2))) << ": " << error << endl;
                        file.close();
                        return 0;
                    }
                    else if (ntohs(*((short*)dataBuf)) == 4) {
                        if (ntohs(*((short*)(dataBuf + 2))) == curblock) {
                            cout << "block acked" << endl;
                            blockAcked = true;
                            curblock++;
                            break;
                        }
                        else if (ntohs(*((short*)(dataBuf + 2))) != curblock) {
                            cout << "wrong ack recieved" << endl;
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
    close(sockfd);
    return 0;
}

//main method, server should take 6 arguments
//server port number, server port address, number of iterations, number of buffers, size of each buffer, and the type of operation to perform
int main(int argc, char* argv[]) {

    int opcode = 0;
    const char* filename;
    int port = PORT;

    if (argc != 3 && argc != 5) {
        cout << "wrong number of arguments" << endl;
        exit(1);
    }
    string flag = argv[1];
    if (flag == "-r") {
        opcode = 1;
    }
    else if (flag == "-w") {
        opcode = 2;
    }
    else {
        cout << "first flag must be -r or -w" << endl;
        exit(1);
    }
    filename = argv[2];

    if (argc == 5) {
        port = stoi(argv[4]);
        if (port < 0) {
            cout << "bad port" << endl;
            exit(1);
        }
    }
    return startTransfer(PORT, filename, opcode);
    //return startTransfer(port, filename, opcode);
}


