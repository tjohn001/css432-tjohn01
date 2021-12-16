//#pragma once
#include "tftp.h"
//#include<bits/stdc++.h>
#include <chrono>
#include <thread>
using namespace std;

/*ReadRequest* findClientInRRQQueue(vector<ReadRequest> queue, sockaddr_in client) {
    for (int i = 0; i < queue.size(); i++) {
        if (queue.at(i).tid == ntohs(client.sin_port)) {
            return &queue.at(i);
        }
    }
    return nullptr;
}
WriteRequest* findClientInWRQQueue(vector<WriteRequest> queue, sockaddr_in client) {
    for (int i = 0; i < queue.size(); i++) {
        if (queue.at(i).tid == ntohs(client.sin_port)) {
            return &queue.at(i);
        }
    }
    return nullptr;
}*/

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

    const int yes = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes));

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
    
    vector<ReadRequest> readVector = vector<ReadRequest>();
    vector<WriteRequest> writeVector = vector<WriteRequest>();

    while (true) {
        int bytesRead = recvfrom(sockfd, (char*)buffer, MAXLINE, MSG_DONTWAIT, (struct sockaddr*)&client, (socklen_t*)&len);
        if (bytesRead > 0) {
            cout << "recieved packet length: " << bytesRead << endl;
            //TODO: add check that ptr < bytes read
            char* ptr = buffer;
            short opcode = *((short*)ptr);
            if (opcode < 1 || opcode > 5) {
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

                if (mode != "octet") {
                    *((short*)buffer) = 5;
                    *((short*)(buffer + 2)) = 4;
                    const char* error = "This server only supports octet mode\0";
                    cout << error << endl;
                    strcpy(buffer + 4, error);
                    sendto(sockfd, (const char*)buffer, 4 + sizeof(error), 0, (const struct sockaddr*)&client, len);
                }

                cout << "opcode: " << opcode << ", filename: " << filename << ", mode: " << mode << endl; 
                if (opcode == 1) {
                    readVector.emplace_back(client, sockfd);
                    readVector.back().start(filename);
                }
                else if (opcode == 2) {
                    writeVector.emplace_back(client, sockfd);
                    writeVector.back().start(filename);
                }
            }
            else if (opcode == 3) {
                for (auto i = writeVector.begin(); i != writeVector.end(); i++) {
                    if (i->tid == ntohs(client.sin_port)) {
                        char in[MAXLINE];
                        bcopy(buffer, in, MAXLINE);
                        i->recieve(in, bytesRead);
                    }
                }
            }
            else if (opcode == 4) {
                for (auto i = readVector.begin(); i != readVector.end(); i++) {
                    if (i->tid == ntohs(client.sin_port)) {
                        char in[4];
                        bcopy(buffer, in, 4);
                        i->recieve(in);
                    }
                }
            }
            else if (opcode == 5) {
                for (auto i = readVector.begin(); i != readVector.end(); i++) {
                    if (i->tid == ntohs(client.sin_port)) {
                        i->curStep = CLOSE;
                        readVector.erase(i);
                    }
                }
                for (auto i = writeVector.begin(); i != writeVector.end(); i++) {
                    if (i->tid == ntohs(client.sin_port)) {
                        i->curStep = CLOSE;
                        writeVector.erase(i);
                    }
                }
            }
            else {
                cout << "other packet type";
            }
        }
        for (auto i = readVector.begin(); i != readVector.end(); i++) {
            STEP step = i->nextStep();
            switch (step) {
            case CLOSE:
                cout << "closing transaction" << endl;
                readVector.erase(i);
                break;
            case RETRY:
                cout << "retrying" << endl;
                i->send();
                break;
            case PROGRESS:
                cout << "progressing" << endl;
                i->send();
                break;
            default:
                break;
            }
        }
        for (auto i = writeVector.begin(); i != writeVector.end(); i++) {
            STEP step = i->nextStep();
            switch (step) {
            case CLOSE:
                writeVector.erase(i);
                break;
            case RETRY:
                i->send();
                break;
            case PROGRESS:
                i->send();
                break;
            }
        }
        cout << "completed loop" << endl;
        this_thread::sleep_for(std::chrono::milliseconds(100)); //for server stability
    }
    close(sockfd); //close socket fd
    return 0;
}