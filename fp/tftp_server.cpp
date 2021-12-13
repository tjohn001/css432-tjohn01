#include <iostream>
#include <cstring>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>   // htonl, htons, inet_ntoa 
#include <arpa/inet.h>    // inet_ntoa 
#include <netdb.h>        // gethostbyname 
#include <unistd.h>       // read, write, close 
#include <strings.h>      // bzero 
#include <netinet/tcp.h>  // SO_REUSEADDR 
#include <pthread.h>
#include <fstream>


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

    int sd = *(int*)ptr;
    struct addrinfo* client = (addrinfo*)(ptr + sizeof(int));
    char buffer[516];
    recvfrom()

}*/

//main method, server should take 2 args - the port number and the number of iterations
int main(int argc, char* argv[]) {
    /*if (argc != 3) { //fail if given wrong number of args
        cerr << "Wrong number of arguments entered" << endl;
        return 1;
    }*/

    //setup server socket
    struct addrinfo hints;
    struct addrinfo* client;//*server, *client;
    int clientSize;
    int status;

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    //add server address info to server
    if ((status = getaddrinfo(NULL, "54949", &hints, &client)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return 1;
    }


    int sd = socket(client->ai_family, client->ai_socktype, client->ai_protocol); //socket file descriptor

    const int yes = 0;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes)); //setup socket

    bind(sd, client->ai_addr, client->ai_addrlen); //bind socket
    
    clientSize = sizeof(client);

    char buffer[516];
    
    int bytesRead = recvfrom(sd, buffer, 516, 0, (sockaddr *) client, (socklen_t*) sizeof(client));
    //TODO: add check that ptr < bytes read
    char* ptr = buffer + 1;
    short opcode = *ptr;
    ptr += 2;
    string filename(ptr);
    ptr += filename.length();
    string mode(ptr);

    cout << filename;

    if (opcode == 1) {
        ifstream file(filename, ifstream::binary);
        int end = file.tellg();
        file.seekg(0, ios::beg);
        int size = end - file.tellg();
        //ptr = buffer;
        char ack[4];
        for (int block = 1; file.tellg() < end; block++) {
            *(buffer) = 3;
            *(buffer + 2) = block;
            int toRead;
            if (block * 512 >= size)
                toRead = size - block * 512;
            else
                toRead = 512;
            file.read(buffer + 4, toRead);
            sendto(sd, buffer, 4 + toRead, 0, (sockaddr*)client, clientSize);

            bytesRead = recvfrom(sd, ack, 4, 0, (sockaddr*)client, (socklen_t*) &clientSize);
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
                sendto(sd, buffer, 4, 0, (sockaddr*)client, clientSize);
                bytesRead = recvfrom(sd, ack, 4, 0, (sockaddr*)client, (socklen_t*)&clientSize);
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
            file.close();
        }
    }
    else if (opcode == 2) {
        ofstream file(filename, ios::binary | std::ofstream::trunc);
        file.seekp(0, ios::beg);
        int data = 0;
        char ack[4];
        memset(&ack, 0, sizeof(ack));
        *(ack + 1) = 4;
        sendto(sd, ack, 4, 0, (sockaddr*)client, clientSize);
        do {
            int bytesRead = recvfrom(sd, buffer, 516, 0, (sockaddr*)client, (socklen_t*)&clientSize);
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

                memset(&ack, 0, sizeof(ack));
                *(ack + 1) = 4;
                bcopy(&block, ack + 2, 2);
                sendto(sd, ack, 4, 0, (sockaddr*)client, clientSize);
                /* resend ack on timeout*/
            }
        } while (data == 512);
        file.close();   
    }




    //accept requests with new thread
    /*while (true) {
        struct sockaddr_storage newSockAddr;
        socklen_t newSockAddrSize = sizeof(newSockAddr);
        int newSd = accept(sd, (struct sockaddr*)&newSockAddr, &newSockAddrSize); //fd of new socket
        int args[] = { newSd, stoi(argv[2]) };
        pthread_t thread;
        int iret = pthread_create(&thread, NULL, &recieve_data, (void*)args); //create new recieve_data thread
        //pthread_join(thread, NULL);
    }*/
    close(sd); //close socket fd
    return 0;
}