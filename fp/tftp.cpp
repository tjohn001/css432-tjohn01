#include "tftp.h"


ReadRequest::ReadRequest(sockaddr_in addr, int fd) {
    memset(&buffer, 0, MAXLINE);
    client = addr;
    tid = ntohs(client.sin_port);
    gettimeofday(&timeSent, NULL);
    len = sizeof(client);
    sockfd = fd;
    curblock = 0;
    lastack = 0;
}

STEP ReadRequest::nextStep(timeval curtime) {
    timeval curtime;
    gettimeofday(&curtime, NULL);
    if (curStep == CLOSE) {
        return CLOSE;
    }
    else if (curStep == START) {
        return START;
    }
    else if (retries >= RETRIES) {
        cout << "Excessive retries, read failed " << "[" << tid << "]" << endl;
        curStep = CLOSE;
        return curStep;
    }
    else if (curBlockSize < 512 && lastack == curblock) { //512 or maxline??
        cout << "Read finished " << "[" << tid << "]" << endl;
        curStep = CLOSE;
        return curStep;
    }
    else if (lastack == curblock) {
        curStep = PROGRESS;
        return curStep;
    }
    else if (curtime.tv_sec - timeSent.tv_sec >= TIMEOUT) {
        curStep = RETRY;
        return curStep;
    }
    else {
        curStep = WAIT;
        return curStep;
    }
}

bool ReadRequest::start(string filename) {
    file = fstream(filename, fstream::in | fstream::ate | fstream::binary);
    if (file.is_open() == false || !file.good()) {
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

bool ReadRequest::send() {
    if (curStep == PROGRESS) {
        curStep = WAIT;
        retries = 0;
        curblock++;
        *((short*)buffer) = htons(3);
        *((short*)(buffer + 2)) = htons(curblock);
        if (curblock * 512 > size) {
            curBlockSize = size - ((curblock - 1) * 512);
        }
        else {
            curBlockSize = 512;
        }
        file.read(buffer + 4, curBlockSize);
        cout << "Sending block " << curblock << " of data " << "[" << tid << "]" << endl;
    }
    else {
        curStep = WAIT;
        retries++;
        cout << "Timed out: resending block " << curblock << " of data " << "[" << tid << "]" << endl;
    }
    int status = (int)sendto(sockfd, (const char*)buffer, 4 + curBlockSize, 0, (const struct sockaddr*)&client, len);
    if (status < 0) {
        cout << "sending error" << endl;
    }
    gettimeofday(&timeSent, NULL);
    return true;
}
//recieves ack: must be 4 bytes
bool ReadRequest::recieve(char* in, int nbytes) {
    retries = 0;
    cout << "recieving ack " << ntohs(*((short*)(in + 2))) << "[" << tid << "]" << endl;
    if (ntohs(*((short*)(in + 2))) == curblock) {
        lastack = ntohs(*((short*)(in + 2)));
        cout << curblock << " block acked" << endl;
    }
    else if (ntohs(*((short*)(in + 2))) != curblock) {
        cout << "wrong ack recieved: " << lastack << endl;
        return false;
    }
    return true;
}


WriteRequest::WriteRequest(sockaddr_in addr, int fd) {
    client = addr;
    tid = ntohs(client.sin_port);
    gettimeofday(&timeSent, NULL);
    len = sizeof(client);
    sockfd = fd;
}
STEP WriteRequest::nextStep(timeval curtime) {
    if (curStep == CLOSE) {
        return CLOSE;
    }
    else if (curStep == START) {
        return START;
    }
    else if (retries >= RETRIES) {
        curStep = CLOSE;
        return curStep;
    }
    else if (lastack == curblock && lastPacketSize < MAXLINE) {
        curStep = CLOSE;
        return curStep;
    }
    else if (lastack < curblock) {
        curStep = PROGRESS;
        return PROGRESS;
    }
    else if (curtime.tv_sec - timeSent.tv_sec >= TIMEOUT) {
        curStep = RETRY;
        return curStep;
    }
    else {
        curStep = WAIT;
        return curStep;
    }
}
bool WriteRequest::start(string filename) {
    file = fstream(filename, fstream::out | fstream::binary | fstream::trunc);
    if (file.is_open() == false) {
        char buffer[MAXLINE];
        *((short*)buffer) = htons(5);
        *((short*)(buffer + 2)) = htons(1);
        const char* error = "Could not open file\0";
        cout << "RRQ: could not open file " << "[" << tid << "]" << endl;
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
bool WriteRequest::send() {
    if (curStep == PROGRESS) {
        curStep = WAIT;
        lastack = curblock;
        retries = 0;
        cout << "ACK block " << lastack << " [" << tid << "]" << endl;
    }
    else {
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
    gettimeofday(&timeSent, NULL);
    retries++;
    return true;
}
bool WriteRequest::recieve(char* in, int nbytes) {
    retries = 0;
    char* readPtr = in + 2;
    cout << "Recieved block " << ntohs(*((short*)readPtr)) << " of data " << "[" << tid << "]" << endl;
    if (ntohs(*((short*)readPtr)) == curblock + 1) {
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
