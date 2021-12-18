
#include <chrono>
#include <thread>
#include "tftp.h"

using namespace std;



pthread_mutex_t read_lock = PTHREAD_MUTEX_INITIALIZER; //locks access to the read vector
pthread_mutex_t write_lock = PTHREAD_MUTEX_INITIALIZER; //locks acces to the write vector

//iterates through each vector and updates their state, calling the relevant method
void* update_transactions(void* ptr) {
    Vectors* vecs = (Vectors*)ptr;
    vector<ReadRequest>* readVector = vecs->read;
    vector<WriteRequest>* writeVector = vecs->write;
    struct timeval curtime;
    while (true) {
        pthread_mutex_lock(&read_lock); //accessing read vector
        gettimeofday(&curtime, NULL); //need current time to check for retries
        for (auto i = readVector->begin(); i != readVector->end() && !readVector->empty(); i++) {
            STEP step = i->nextStep(curtime); //get next operation
            switch (step) {
            case CLOSE:
                cout << "Closing transaction " << "[" << i->tid << "]" << endl;
                i = readVector->erase(i);
                break;
            case RETRY:
                i->send();
                break;
            case PROGRESS:
                i->send();
                break;
            default:
                break;
            }
        }
        pthread_mutex_unlock(&read_lock);
        pthread_mutex_lock(&write_lock); //accessing write vector
        gettimeofday(&curtime, NULL); //get current time to check for retries
        for (auto i = writeVector->begin(); i != writeVector->end() && !writeVector->empty(); i++) {
            STEP step = i->nextStep(curtime);
            switch (step) {
            case CLOSE:
                cout << "Closing transaction " << "[" << i->tid << "]" << endl;
                i = writeVector->erase(i);
                break;
            case RETRY:
                i->send();
                break;
            case PROGRESS:
                i->send();
                break;
            }
        }
        pthread_mutex_unlock(&write_lock);
    }
}

//main method, server should take 2 args - the port number and the number of iterations
int main(int argc, char* argv[]) {

    int port = PORT;
    if (argc == 3 && string(argv[1]) == "-p") { //allow to set port
        port = stoi(string(argv[2]));
        cout << "port entered: " << string(argv[2]) << endl;
        if (port < 0) {
            cout << "bad port " << port << endl;
            exit(1);
        }  
    }
    cout << "Recieving on port " << port << endl;
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
    server.sin_port = htons(port);

    // Bind the socket with the server address
    if (bind(sockfd, (const struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    vector<ReadRequest> readVector = vector<ReadRequest>();
    vector<WriteRequest> writeVector = vector<WriteRequest>();

    pthread_t thread1;

    struct Vectors vecs;
    vecs.read = &readVector;
    vecs.write = &writeVector;

    pthread_create(&thread1, NULL, update_transactions, (void*)&vecs);

    while (true) {
        int bytesRead = recvfrom(sockfd, (char*)buffer, MAXLINE, MSG_DONTWAIT, (struct sockaddr*)&client, (socklen_t*)&len);
        if (bytesRead > 0) {
            char* ptr = buffer;
            short opcode = ntohs(*((short*)ptr));


            if (opcode < 1 || opcode > 5) {
                *((short*)buffer) = htons(5);
                *((short*)(buffer + 2)) = htons(4);
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
                
                if (filename.find('/') != -1 || filename.find('\\') != -1) { //do not allow / or \ in filename
                    *((short*)buffer) = htons(5);
                    *((short*)(buffer + 2)) = htons(3);
                    const char* error = "Attempted to access file path\0";
                    cout << error << endl;
                    strcpy(buffer + 4, error);
                    sendto(sockfd, (const char*)buffer, 4 + sizeof(error), 0, (const struct sockaddr*)&client, len);
                }
                else if (mode != "octet") {
                    *((short*)buffer) = htons(5);
                    *((short*)(buffer + 2)) = htons(4);
                    const char* error = "This server only supports octet mode\0";
                    cout << error << endl;
                    strcpy(buffer + 4, error);
                    sendto(sockfd, (const char*)buffer, 4 + sizeof(error), 0, (const struct sockaddr*)&client, len);
                }
                else if (opcode == 1) {
                    pthread_mutex_lock(&read_lock);
                    bool tidExists = false;
                    for (auto i = readVector.begin(); i != readVector.end(); i++) {
                        if (i->tid == ntohs(client.sin_port)) {
                            tidExists = true;
                            //i->retries = 0;
                            cout << "tid already has connection" << endl;
                            break;
                        }
                    }
                    if (!tidExists) {
                        readVector.emplace_back(client, sockfd);
                        readVector.back().start(filename);
                    }
                    pthread_mutex_unlock(&read_lock);
                }
                else if (opcode == 2) {
                    pthread_mutex_lock(&write_lock);
                    bool tidExists = false;
                    for (auto i = writeVector.begin(); i != writeVector.end(); i++) {
                        if (i->tid == ntohs(client.sin_port)) {
                            tidExists = true;
                            //i->retries = 0;
                            cout << "tid already has connection" << endl;
                            break;
                        }
                    }
                    if (!tidExists) {
                        writeVector.emplace_back(client, sockfd);
                        writeVector.back().start(filename);
                    }
                    pthread_mutex_unlock(&write_lock);
                }
            }
            else if (opcode == 3) {
                pthread_mutex_lock(&write_lock);
                for (auto i = writeVector.begin(); i != writeVector.end(); i++) {
                    if (i->tid == ntohs(client.sin_port)) {
                        char in[MAXLINE];
                        bcopy(buffer, in, MAXLINE);
                        i->recieve(in, bytesRead);
                        break;
                    }
                }
                pthread_mutex_unlock(&write_lock);
            }
            else if (opcode == 4) {
                pthread_mutex_lock(&read_lock);
                for (auto i = readVector.begin(); i != readVector.end(); i++) {
                    if (i->tid == ntohs(client.sin_port)) {
                        char in[4];
                        bcopy(buffer, in, 4);
                        i->recieve(in, 4);
                        break;
                    }
                }
                pthread_mutex_unlock(&read_lock);
            }
            else if (opcode == 5) {
                pthread_mutex_lock(&read_lock);
                for (auto i = readVector.begin(); i != readVector.end() && !readVector.empty(); i++) {
                    if (i->tid == ntohs(client.sin_port)) {
                        i->curStep = CLOSE;
                        i = readVector.erase(i);
                        break;
                    }
                }
                pthread_mutex_unlock(&read_lock);
                pthread_mutex_lock(&write_lock);
                for (auto i = writeVector.begin(); i != writeVector.end() && !writeVector.empty(); i++) {
                    if (i->tid == ntohs(client.sin_port)) {
                        i->curStep = CLOSE;
                        i = writeVector.erase(i);
                        break;
                    }
                }
                pthread_mutex_unlock(&write_lock);
            }
        }
        //this_thread::sleep_for(std::chrono::milliseconds(10)); //for server stability
    }
    close(sockfd); //close socket fd
    return 0;
}