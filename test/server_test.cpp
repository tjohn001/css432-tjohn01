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

using namespace std;

int PORT = 54949;

int main() {
    struct sockaddr_in server, client;
    int status, cliSize;

    memset(&server, 0, sizeof(server));
    memset(&client, 0, sizeof(client)); // make sure the struct is empty

    int sd = socket(AF_INET, SOCK_DGRAM, 0); //socket file descriptor

    server.sin_family = AF_INET; // IPv4
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    bind(sd, (const struct sockaddr*)&server, sizeof(server));

    cliSize = sizeof(client);

    char buffer[7];

    int n = recvfrom(sd, buffer, 7, 0, (struct sockaddr*)&client, (socklen_t*)&cliSize);
    string res(buffer);
    cout << res;
    sendto(sd, buffer, 7, 0, (struct sockaddr*)&client, cliSize);
}