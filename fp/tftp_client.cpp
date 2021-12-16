
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

    const int yes = 0;

    memset(&server, 0, sizeof(server));

    // Filling server information
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr(HOST_ADDRESS);

    struct timeval timeSent;
    timeSent.tv_sec = TIMEOUT; /* seconds */
    timeSent.tv_usec = 0;

    //if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval*)&timeSent, sizeof(timeSent) < 0))
        //cout << "Cannot Set SO_RCVTIMEO for socket: " << strerror(errno) << endl;

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
        sendto(sockfd, buffer, ptr - buffer, 0, (const struct sockaddr*)&server, len);
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
            else if (ntohs(*((short*)buffer)) == 5) {
                cout << "error";
                return -1;
            }
            else if (ntohs(*((short*)buffer)) != 3) {
                cout << "wrong packet type: " << ntohs(*((short*)buffer)) << endl;
            }
            else {
                char* readPtr = buffer + 2;
                short curblock = ntohs(*((short*)readPtr));
                readPtr += 2;
                file.write(readPtr, bytesRead - 4);
                data = bytesRead - 4;
                
                char ack[4];
                *((short*)ack) = htons(4);
                *((short*)(ack + 2)) = htons(curblock);
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
        char ack[MAXLINE];
        int bytesRead = 0;
        int retries = 0;
        struct timeval start_time, cur_time; //timer
        gettimeofday(&start_time, NULL);
        gettimeofday(&cur_time, NULL);
        bool transAcked = false;
        while(retries < RETRIES && !transAcked) {
            sendto(sockfd, buffer, ptr - buffer, 0, (const struct sockaddr*)&server, len);
            gettimeofday(&start_time, NULL);
            while (cur_time.tv_sec - start_time.tv_sec < TIMEOUT && !transAcked) {
                bytesRead = (int)recvfrom(sockfd, (char*)ack, MAXLINE, MSG_DONTWAIT, (struct sockaddr*)&server, (socklen_t*)&len);
                if (bytesRead > 4) {
                    retries = 0;
                    if (ntohs(*((short*)ack)) == 5) {
                        cout << "recieved error" << endl;
                        return 0;
                    }
                    else if (ntohs(*((short*)ack)) != 4) {
                        cout << "wrong packet type" << ntohs(*((short*)ack)) << endl;
                    }
                    else if (ntohs(*((short*)(ack + 2))) != 0) {
                        cout << "ack not 0: " << ntohs(*((short*)(ack + 2))) << endl;
                    }
                    else if (ntohs(*((short*)(ack + 2))) == 0) {
                        cout << "ack 0 recieved" << endl;
                        transAcked = true;
                    }
                }
                gettimeofday(&cur_time, NULL);
            }
            if (!transAcked) {
                cout << "timed out waiting for ack, retrying..." << endl;
            }
            retries++;
        }
        if (retries == RETRIES || !transAcked) {
            cout << "Retry failed, ending connection" << endl;
            return 0;
        }

        ifstream file(filename, ios::ate | ios::binary);
        long end = file.tellg();
        file.seekg(0, ios::beg);
        long size = end - file.tellg();
        cout << "start: " << file.tellg() << " end: " << end << " size: " << size << endl;
        struct sockaddr_in data;
        memset(&data, 0, sizeof(data));
        int len = sizeof(server);
        int dataLen = sizeof(data);
        char buffer[MAXLINE];
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
            cout << "reading file" << endl;
            file.read(buffer + 4, toRead); 
            bool blockAcked = false;
            for (int i = 0; i < RETRIES && !blockAcked; i++) {
                cout << "sending bytes: " << toRead << "in block" << curblock << endl;
                int status = sendto(sockfd, (const char*)buffer, 4 + toRead, 0, (const struct sockaddr*)&server, len);
                if (status < 0) {
                    cout << "sending error" << endl;
                    continue;
                }
                gettimeofday(&start_time, NULL);
                gettimeofday(&cur_time, NULL);
                while (cur_time.tv_sec - start_time.tv_sec < TIMEOUT && !blockAcked) {
                    gettimeofday(&cur_time, NULL);
                    int bytesRead = (int)recvfrom(sockfd, (char*)dataBuf, MAXLINE, MSG_DONTWAIT, (struct sockaddr*)&data, (socklen_t*)&dataLen);
                    if (bytesRead >= 4) {
                        cout << "packet recieved" << endl;
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
                    }
                    if (!blockAcked) {
                        cout << "timed out waiting for ack, retrying..." << endl;
                    }
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
    /*if (argc != 7) { //check that correct # of arguments were entered
        cerr << "Wrong number of arguments entered" << endl;
        exit(1);
    }
    if (stoi(argv[4]) * stoi(argv[5]) != 1500) { //check that resulting buffer size = 1500
        cerr << "nbufs * bufsize must equal 1500" << endl;
        exit(1);
    }*/

    return startTransfer("54948", "test.txt", 2);

    //return startTransfer(argv[1], argv[2], stoi(argv[3]), stoi(argv[4]), stoi(argv[5]), stoi(argv[6]));
}


