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

int MAXLINE = 516, PORT = 51949;

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
    struct addrinfo* client = (addrinfo*)(ptr + sizeof(int));
    char buffer[MAXLINE];
    recvfrom()

}*/

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
    if (bind(sockfd, (const struct sockaddr*)&server,
        sizeof(server)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    int len;

    len = sizeof(client); //len is value/resuslt
    
    int bytesRead = recvfrom(sockfd, (char*)buffer, MAXLINE, MSG_WAITALL, (struct sockaddr*)&client, (socklen_t*)&len);
    //TODO: add check that ptr < bytes read
    char* ptr = buffer;
    short opcode = *((short*)ptr);
    ptr += 2;
    string filename(ptr);
    ptr += filename.length();
    string mode(ptr);

    for (int i = 0; i < 25; i++) {
        cout << buffer[i];
    }
    cout << endl;

    cout << "opcode: " << opcode << ", filename: " << filename << ", mode: " << mode;

    if (opcode == 1) {
        cout << "RRQ :" << filename << endl;
        ifstream file(filename, ios::ate | ios::binary);
        cout << "file opened" << endl;
        int end = file.tellg();
        file.seekg(0, ios::beg);
        int size = end - file.tellg();
        //ptr = buffer;
        char ack[4];
        cout << "start: " << file.tellg() << " end: " << end << " size: " << size;
        for (int block = 1; file.tellg() < end; block++) {
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

            //if last block is exactly 512 bytes, send exta size 0 block
            if (block * 512 == size) {
                block++;
                *((short*)(buffer + 2)) = block;
                sendto(sockfd, buffer, 4, MSG_CONFIRM, (const struct sockaddr*)&client, len);
                bytesRead = recvfrom(sockfd, ack, 4, MSG_WAITALL, (struct sockaddr*)&client, (socklen_t*)&len);
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
    else if (opcode == 2) {
        ofstream file(filename, ios::binary | ios::trunc);
        file.seekp(0, ios::beg);
        int data = 0;
        char ack[4];
        memset(&ack, 0, sizeof(ack));
        *((short*)ack) = 4;
        sendto(sockfd, ack, 4, MSG_CONFIRM, (const struct sockaddr*)&client, len);
        do {
            int bytesRead = recvfrom(sockfd, buffer, MAXLINE, MSG_WAITALL, (struct sockaddr*)&client, (socklen_t*)&len);
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
                *((short*)ack) = 4;
                bcopy(&block, ack + 2, 2);
                sendto(sockfd, ack, 4, MSG_CONFIRM, (const struct sockaddr*)&client, len);
                /* resend ack on timeout*/
            }
        } while (data == 512);
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