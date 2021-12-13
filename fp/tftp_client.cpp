/*
* Taylor Johnston
* Client side application that sends a given number of iterations of 1500 bytes seperated into given buffer sizes using one of 3 methods
* total buffer size adds up to 1500 bytes to given port on a server, then recieves number of bytesRead operations the server performed
*/
#include <iostream>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fstream>;


using namespace std;

//method for handling creating connection to server
//takes in port #, server address, number of iterations to run, number of buffers, size of buffers, and the type of sending method to use
int createConnection(const char* port, const char* address, const char* filename, const short opcode) {

    //setup server socket
    struct addrinfo hints;
    struct addrinfo* server;
    int status, serverSize;

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM; // TCP stream sockets

    //add server address info to server
    if ((status = getaddrinfo(address, port, &hints, &server)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return 1;
    }

    int sd = socket(server->ai_family, server->ai_socktype, server->ai_protocol); //socket file descriptor

    serverSize = sizeof(server);

    char buffer[516];

    char* ptr = buffer;
    *ptr = 0;
    ptr++;
    *ptr = opcode;
    ptr += sizeof(opcode);
    strcpy(ptr, filename);
    ptr += sizeof(filename);
    *ptr = 0;
    ptr++;
    strcpy(ptr, "octet");
    ptr += sizeof("octet");
    *ptr = 0;
    
    sendto(sd, buffer, ptr - buffer, 0, (sockaddr*)server, serverSize);

    if (opcode == 1) {
        ofstream file (filename, ios::binary | std::ofstream::trunc);
        file.seekp(0, ios::beg);
        int data = 0;
        do {
            int bytesRead = recvfrom(sd, buffer, 516, 0, (sockaddr*)server, (socklen_t*)&serverSize);
            if (bytesRead < 2) {
                cout << "read error";
                data = 0;
            }
            else if (buffer[1] == 5) {
                cout << "error";
                data = 0;
            }
            else if (buffer[1] != 3) {
                cout << "wrong packet type";
                data = 0;
            }
            else {
                char* readPtr = buffer + 2;
                short int block = *readPtr;
                readPtr += 2;
                file.write(readPtr, bytesRead - 4);
                data = bytesRead - 4;

                char ack[4];
                char* tempPtr = ack;
                *(ack) = 4;
                *(ack + 2) = block;
                sendto(sd, ack, 4, 0, (sockaddr*)server, serverSize);
                /* resend ack on timeout*/
            }
        } while (data == 512);
        file.close();
    }
    else if (opcode == 2) {
        char ack[4];
        int bytesRead = recvfrom(sd, ack, 4, 0, (sockaddr*)server, (socklen_t*)&serverSize);
        if (bytesRead != 4) {
            cout << "packet error";
        }
        else if (*ack != 4) {
            cout << "wrong packet type";
        }
        else if (*(ack + 2) != 0) {
            cout << "wrong starting ack";
        }
        else {
            ifstream file(filename, ifstream::binary);
            int end = file.tellg();
            file.seekg(0, ios::beg);
            int size = end - file.tellg();
            //ptr = buffer;
            for (int block = 1; file.tellg() < end; block++) {
                *(buffer) = 3;
                *(buffer + 2) = block;
                int toRead;
                if (block * 512 >= size) 
                    toRead = size - block * 512;
                else 
                    toRead = 512;
                file.read(buffer + 4, toRead);
                sendto(sd, buffer, 4 + toRead, 0, (sockaddr*)server, serverSize);

                bytesRead = recvfrom(sd, ack, 4, 0, (sockaddr*)server, (socklen_t*)&serverSize);
                if (bytesRead != 4) {
                    cout << "packet error";
                }
                else if (*ack != 4) {
                    cout << "wrong packet type";
                }
                else if (*((short*)ack + 2) != block) {
                    cout << "wrong ack #";
                }
                
                //if last block is exactly 512 bytes, send exta size 0 block
                if (block * 512 == size) {
                    *(buffer + 2) = block + 1;
                    sendto(sd, buffer, 4, 0, (sockaddr*)server, (serverSize));
                    bytesRead = recvfrom(sd, ack, 4, 0, (sockaddr*)server, (socklen_t*)&serverSize);
                    if (bytesRead != 4) {
                        cout << "packet error";
                    }
                    else if (*ack != 4) {
                        cout << "wrong packet type";
                    }
                    else if (*(ack + 2) != block) {
                        cout << "wrong ack #";
                    }
                }

            }
            file.close();
        }
    }
    close(sd);
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

    return createConnection("54949", "csslab9.uwb.edu", "test.txt", 1);

    //return createConnection(argv[1], argv[2], stoi(argv[3]), stoi(argv[4]), stoi(argv[5]), stoi(argv[6]));
}


