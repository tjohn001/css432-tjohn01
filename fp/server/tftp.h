#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>   // htonl, htons, inet_ntoa 
#include <arpa/inet.h>    // inet_ntoa 
#include <netdb.h>        // gethostbyname 
#include <unistd.h>       // read, write, close 
#include <sys/time.h>
#include <vector>


using namespace std;

#define MAXLINE 516
#define PORT 51948
#define RETRIES 10
#define TIMEOUT 2

enum STEP { CLOSE, RETRY, WAIT, PROGRESS, START }; //phases of operations performed by Transactions

//base class for read/write transactions
class Transaction {
public:
    sockaddr_in client; //client
    fstream file; //file to operate on
    int retries = 0, tid, len, sockfd; //current count of retries, client tid, size of client, and socket fd
    short curblock, lastack; //current data block and last ACK recieved
    timeval timeSent; //time that last packet was sent
    STEP curStep = START; //current operation being performed
};
//class for RRQs
class ReadRequest : public Transaction {
public:
    char buffer[MAXLINE]; //buffer to read file into
    int curBlockSize = 512; //size of current block being sent
    long size; //total size of file
    ReadRequest(sockaddr_in addr, int fd); //constructor
    STEP nextStep(timeval curtime); //calculate next operation
    bool start(string filename); //load file and initialize variables
    bool send(); //send data packet
    bool recieve(char* in, int nbytes); //recieves 4 byte ACK
};
//class for WRQs
class WriteRequest : public Transaction {
public:
    int lastPacketSize = MAXLINE; //size of last packet recieved
    WriteRequest(sockaddr_in addr, int fd); //constructor
    STEP nextStep(timeval curtime); //calculate next operation
    bool start(string filename); //open file and initialize variables
    bool send(); //send ACK
    bool recieve(char* in, int nbytes); //recieves DATA packet
};

//struct used for passing vectors to thread
struct Vectors {
public:
    vector<ReadRequest>* read;
    vector<WriteRequest>* write;
};