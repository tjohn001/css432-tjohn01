#include "tftp.h"

//constructor implementation
ReadRequest::ReadRequest(sockaddr_in addr, int fd) {
    memset(&buffer, 0, MAXLINE); //clear buffer
    client = addr; //assign client
    tid = ntohs(client.sin_port); //save client's tid
    gettimeofday(&timeSent, NULL); //start clock
    len = sizeof(client); 
    sockfd = fd; //set socket fd
    curblock = 0;
    lastack = 0;
}

//determines what step should be taken next
STEP ReadRequest::nextStep(timeval curtime) {
    if (curStep == CLOSE) { //if transaction is closing, don't take any other actions
        return CLOSE;
    }
    else if (curStep == START) { //don't interupt process while it's starting
        return START;
    }
    else if (retries >= RETRIES) { //close if connection failed
        cout << "Excessive retries, read failed " << "[" << tid << "]" << endl;
        curStep = CLOSE;
        return curStep;
    }
    else if (curBlockSize < 512 && lastack == curblock) { //close if end of transaction reached & ack recieved 
        cout << "Read finished " << "[" << tid << "]" << endl;
        curStep = CLOSE;
        return curStep;
    }
    else if (lastack == curblock) { //if ack for last block was recieved, send next block
        curStep = PROGRESS;
        return curStep;
    }
    else if (curtime.tv_sec - timeSent.tv_sec >= TIMEOUT) { //if timeout exceeded, retry sending data
        curStep = RETRY;
        return curStep;
    }
    else { //wait for current operation to finish or timeout to expire
        curStep = WAIT;
        return curStep;
    }
}

//initializes the read request
bool ReadRequest::start(string filename) {
    file = fstream(filename, fstream::in | fstream::ate | fstream::binary); //open file
    if (file.is_open() == false || !file.good()) { //if file didn't open, send error
        *((short*)buffer) = htons(5);
        *((short*)(buffer + 2)) = htons(1);
        const char* error = "Could not open file\0";
        cout << error << endl;
        strcpy(buffer + 4, error);
        sendto(sockfd, (const char*)buffer, 4 + sizeof(error), 0, (const struct sockaddr*)&client, len);
        curStep = CLOSE;
        return false;
    }
    long end = file.tellg();
    file.seekg(0, ios::beg);
    size = end - file.tellg();
    cout << "Starting read transaction: " << filename << ", " << tid << endl;
    curStep = PROGRESS;
    return true;
}

//send data to client
bool ReadRequest::send() {
    if (curStep == PROGRESS) { //send next block
        curStep = WAIT;
        retries = 0;
        curblock++;
        *((short*)buffer) = htons(3); //DATA
        *((short*)(buffer + 2)) = htons(curblock); //block #
        if (curblock * 512 > size) { //get size of block
            curBlockSize = size - ((curblock - 1) * 512);
        }
        else {
            curBlockSize = 512;
        }
        file.read(buffer + 4, curBlockSize); //read file into buffer
        cout << "Sending block " << curblock << " of data " << "[" << tid << "]" << endl;
    }
    else { //resend last block
        curStep = WAIT;
        retries++;
        cout << "Timed out: resending block " << curblock << " of data " << "[" << tid << "]" << endl;
    }
    int status = (int)sendto(sockfd, (const char*)buffer, 4 + curBlockSize, 0, (const struct sockaddr*)&client, len); //send the data
    if (status < 0) {
        cout << "sending error" << endl;
    }
    gettimeofday(&timeSent, NULL);
    return true;
}
//recieves ack: must be 4 bytes
bool ReadRequest::recieve(char* in, int nbytes) {
    retries = 0; //reset retries counter
    cout << "recieving ack " << ntohs(*((short*)(in + 2))) << "[" << tid << "]" << endl;
    if (ntohs(*((short*)(in + 2))) == curblock) { //if correct ack recieved, update 
        lastack = ntohs(*((short*)(in + 2)));
        cout << curblock << " block acked" << endl;
    }
    else if (ntohs(*((short*)(in + 2))) != curblock) { //discard out of order acks
        cout << "wrong ack recieved: " << lastack << endl;
        return false;
    }
    return true;
}

//initialize write request
WriteRequest::WriteRequest(sockaddr_in addr, int fd) {
    client = addr;
    tid = ntohs(client.sin_port);
    gettimeofday(&timeSent, NULL);
    len = sizeof(client);
    sockfd = fd;
}
//choose next operation to perform
STEP WriteRequest::nextStep(timeval curtime) {
    if (curStep == CLOSE) {
        return CLOSE;
    }
    else if (curStep == START) { //don't interupt process while it's starting
        return START;
    }
    else if (retries >= RETRIES) { //if retries exceeded, close transaction
        cout << "Excessive retries, write failed " << "[" << tid << "]" << endl;
        curStep = CLOSE;
        return curStep;
    }
    else if (lastack == curblock && lastPacketSize < MAXLINE) { //close after ack for last packet sent
        cout << "Write finished " << "[" << tid << "]" << endl;
        curStep = CLOSE;
        return curStep;
    }
    else if (lastack < curblock) { //send ack after recieving next data block
        curStep = PROGRESS;
        return PROGRESS;
    }
    else if (curtime.tv_sec - timeSent.tv_sec >= TIMEOUT) { //timedout, retry
        curStep = RETRY;
        return curStep;
    }
    else { //don't interrupt other operations, wait for timeout
        curStep = WAIT;
        return curStep;
    }
}
//intitialize write request
bool WriteRequest::start(string filename) {
    if (access(filename.c_str(), F_OK) != -1) { //file already exists
        char buffer[MAXLINE];
        *((short*)buffer) = htons(5);
        *((short*)(buffer + 2)) = htons(2);
        const char* error = "File already exists\0";
        cout << "WRQ: attempted to write to existing file " << "[" << tid << "]" << endl;
        strcpy(buffer + 4, error);
        sendto(sockfd, (const char*)buffer, 4 + sizeof(error), 0, (const struct sockaddr*)&client, len);
        curStep = CLOSE;
        return false;
    }
    file = fstream(filename, fstream::out | fstream::binary | fstream::trunc);
    if (file.is_open() == false) { //could not open file
        char buffer[MAXLINE];
        *((short*)buffer) = htons(5);
        *((short*)(buffer + 2)) = htons(1);
        const char* error = "Could not open file\0";
        cout << "WRQ: could not open file " << "[" << tid << "]" << endl;
        strcpy(buffer + 4, error);
        sendto(sockfd, (const char*)buffer, 4 + sizeof(error), 0, (const struct sockaddr*)&client, len);
        curStep = CLOSE;
        return false;
    }
    file.seekg(0, ios::beg);
    cout << "Starting write transaction: " << filename << "[" << tid << "]" << endl;
    lastack = -1;
    curblock = 0;
    curStep = PROGRESS;
    return true;
}
bool WriteRequest::send() { //send ack
    if (curStep == PROGRESS) { //reset retries, update lastack
        curStep = WAIT;
        lastack = curblock;
        retries = 0;
        cout << "ACK block " << lastack << " [" << tid << "]" << endl;
    }
    else { //retry
        curStep = WAIT;
        cout << "Timed out, resending ACK " << lastack << " [" << tid << "]" << endl;
    }
    char ack[4];
    *((short*)(ack)) = htons(4);
    *((short*)(ack + 2)) = htons(lastack);
    int status = (int)sendto(sockfd, (const char*)ack, 4, 0, (const struct sockaddr*)&client, len);
    if (status < 0) {
        cout << "sending error" << endl;
    }
    gettimeofday(&timeSent, NULL); //update timer
    retries++;
    return true;
}
//recieve data
bool WriteRequest::recieve(char* in, int nbytes) {
    retries = 0;
    char* readPtr = in + 2;
    cout << "Recieved block " << ntohs(*((short*)readPtr)) << " of data " << "[" << tid << "]" << endl;
    if (ntohs(*((short*)readPtr)) == curblock + 1) { //if new block, update status
        curblock = ntohs(*((short*)readPtr));
        lastPacketSize = nbytes;
        readPtr += 2;
        file.write(readPtr, nbytes - 4);
        return true;
    }
    //after recieving out of order data, need to resend ack
    else {
        send();
    }
    return false;
}
