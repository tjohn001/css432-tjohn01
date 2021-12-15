
//#pragma once
#include "tftp.h"

using namespace std;

//method for handling creating connection to server
//takes in port #, server address, number of iterations to run, number of buffers, size of buffers, and the type of sending method to use
int createConnection(const char* port, const char* filename, const short opcode) {

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
    server.sin_addr.s_addr = INADDR_ANY;

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
        do {
            cout << "wait for packet" << endl;
            int bytesRead = recvfrom(sockfd, buffer, MAXLINE, MSG_WAITALL, (struct sockaddr*)&server, (socklen_t*)&len);
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
                
                char ack[4];
                char* tempPtr = ack;
                *((short*)ack) = 4;
                *((short*)(ack + 2)) = block;
                cout << "read " << bytesRead << " send ack " << block << endl;
                sendto(sockfd, ack, 4, MSG_CONFIRM, (const struct sockaddr*)&server, len);
                /* resend ack on timeout*/
            }
        } while (data == 512);
        cout << "done" << endl;
        file.close();
    }
    else if (opcode == 2) {
        cout << "WRQ :" << filename << endl;
        char ack[4];
        int bytesRead =  recvfrom(sockfd, ack, 4, MSG_WAITALL, (struct sockaddr*)&server, (socklen_t*)&len);
        cout << "ack recieved" << endl;
        if (bytesRead != 4) {
            cout << "packet error" << endl;
        }
        else if (*((short*)ack) != 4) {
            cout << "wrong packet type" << *((short*)ack) << endl;
        }
        else if (*((short*)(ack + 2)) != 0) {
            cout << "ack not 0: " << *((short*)(ack + 2)) << endl;
        }
        ifstream file(filename, ios::ate | ios::binary);
        cout << "file opened" << endl;
        int end = file.tellg();
        file.seekg(0, ios::beg);
        int size = end - file.tellg();
        //ptr = buffer;
        
        cout << "start: " << file.tellg() << " end: " << end << " size: " << size;
        for (int block = 1; file.tellg() < end; block++) {
            *((short*)buffer) = 3;
            *((short*)(buffer + 2)) = block;
            int toRead;
            if (block * 512 > size)
                toRead = size - ((block - 1) * 512);
            else
                toRead = 512;
            cout << "reading file" << endl;
            file.read(buffer + 4, toRead);
            cout << "sending bytes: " << toRead << endl;
            cout << "bytes sent: " << (int)sendto(sockfd, (const char*)buffer, 4 + toRead, MSG_CONFIRM, (const struct sockaddr*)&server, len) << endl;
            cout << "block sent: " << *((short*)(buffer + 2)) << endl;
            int bytesRead = recvfrom(sockfd, ack, 4, MSG_WAITALL, (struct sockaddr*)&server, (socklen_t*)&len);
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

            //if last block is exactly 512 bytes, send exta size 0 block
            if (block * 512 == size) {
                block++;
                *((short*)(buffer + 2)) = block;
                sendto(sockfd, buffer, 4, MSG_CONFIRM, (const struct sockaddr*)&server, len);
                bytesRead = recvfrom(sockfd, ack, 4, MSG_WAITALL, (struct sockaddr*)&server, (socklen_t*)&len);
                if (bytesRead != 4) {
                    cout << "packet error";
                }
                else if (*((short*)ack) != 4) {
                    cout << "wrong packet type";
                }
                else if (*((short*)(ack + 2)) != block) {
                    cout << "wrong ack #";
                }
            }
        }
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

    return createConnection("54948", "test.txt", 1);

    //return createConnection(argv[1], argv[2], stoi(argv[3]), stoi(argv[4]), stoi(argv[5]), stoi(argv[6]));
}


