//#pragma once
#include "tftp.h"

using namespace std;

int sendFile(string filename, sockaddr_in recvaddr, int sockfd) {
    int tid = ntohs(recvaddr.sin_port);
    ifstream file(filename, ios::ate | ios::binary);
    int end = file.tellg();
    file.seekg(0, ios::beg);
    int size = end - file.tellg();
    cout << "start: " << file.tellg() << " end: " << end << " size: " << size << endl;
    struct sockaddr_in data;
    memset(&data, 0, sizeof(data));
    int len = sizeof(recvaddr);
    int dataLen = sizeof(data);
    char buffer[MAXLINE];
    char dataBuf[MAXLINE];
    int toRead, block = 1;
    do {
        *((short*)buffer) = 3;
        *((short*)(buffer + 2)) = block;
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
                cout << "waiting for packet" << endl;
                int bytesRead = recvfrom(sockfd, (char*)dataBuf, MAXLINE, MSG_DONTWAIT, (struct sockaddr*)&data, (socklen_t*)&dataLen);
                if (bytesRead >= 4) {
                    if (tid == ntohs(data.sin_port)) {
                        if (*((short*)dataBuf) == 5) {
                            string error(dataBuf + 4);
                            cout << "Error " << *((short*)(dataBuf + 2)) << ": " << error << endl;
                            file.close();
                            return -1;
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
    } while (toRead == 512);
    file.close();
    return 0;
}

int writeFile(string filename, sockaddr_in recvaddr, int sockfd) {
    return -1;
}

void processPacket(char* buffer, struct sockaddr_in client, int len, int sockfd) {
    int bytesRead = recvfrom(sockfd, (char*)buffer, MAXLINE, MSG_DONTWAIT, (struct sockaddr*)&client, (socklen_t*)&len);
    if (bytesRead <= 0) return;
    //TODO: add check that ptr < bytes read
    char* ptr = buffer;
    short opcode = *((short*)ptr);
    if (opcode < 1 || opcode > 7) {
        *((short*)buffer) = 5;
        *((short*)(buffer + 2)) = 4;
        const char* error = "Uknown operation\0";
        cout << error << endl;
        strcpy(buffer + 4, error);
        sendto(sockfd, (const char*)buffer, 4 + sizeof(error), 0, (const struct sockaddr*)&client, len);
    }
    else if (opcode == 1 || opcode == 2) {
        ptr += 2;
        string filename(ptr);
        ptr += filename.length() + 1;
        string mode(ptr);

        cout << "opcode: " << opcode << ", filename: " << filename << ", mode: " << mode << endl;
        int pid = fork();
        if (pid != 0) { //let child handle process
            if (opcode == 1) {
                sendFile(filename, client, sockfd);
            }
            else if (opcode == 2) {
                writeFile(filename, client, sockfd);
            }
            exit(0);
        }
        if (pid < 0) {
            cout << "fork error" << endl;
        }
    }
    else {
        cout << "other packet type";
    }
}

//main method, server should take 2 args - the port number and the number of iterations
int main(int argc, char* argv[]) {
    //setup server socket
    int sockfd;
    char buffer[MAXLINE];
    struct sockaddr_in server, client;
    int len = sizeof(client); //len is value/resuslt
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
        //cout << "Cannot Set SO_SNDTIMEO for socket" << endl;
    
    while (true) {
        processPacket(buffer, client, len, sockfd);
    }
    close(sockfd); //close socket fd
    return 0;
}