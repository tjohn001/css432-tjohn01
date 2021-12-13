#include <iostream>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fstream>

using namespace std;

int PORT = 545949;

int main() {
    //struct addrinfo hints;
    //struct addrinfo_in *server, *client;
    struct sockaddr_in server;
    int status, sersize;

    memset(&server, 0, sizeof(server));
    //memset(&client, 0, sizeof(client)); // make sure the struct is empty
    //hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    //hints.ai_socktype = SOCK_DGRAM; // UPD datagram

    /*//add server address info to server
    if ((status = getaddrinfo("csslab9.uwb.edu", "54988", &hints, &server)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return 1;
    }*/

    int sd = socket(AF_INET, SOCK_DGRAM, 0); //socket file descriptor

    cout << sd << endl;

    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;

    sersize = sizeof(server);

    char* buffer = "message";

    sendto(sd, buffer, 7, 0, (struct sockaddr*)&server, sersize);
    cout << "message sent" << endl;
    int n = recvfrom(sd, buffer, 7, 0, (struct sockaddr*)&server, (socklen_t*)&sersize);
    string res(buffer);
    cout << res;
}