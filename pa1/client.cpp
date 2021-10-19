#include <iostream>

using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 6) {
        cerr << "Wrong number of arguments entered" << endl;
        return 1;
    }

    
}

int createConnection(char* argv) {


    const int iterations = (int)argv[2], nbufs = (int)argv[3], bufsize = (int)argv[4], type = (int)argv[5];

    if (nbufs * bufsize != 1500) {
        cerr << "nbufs * bufsize must equal 1500" << endl;
        return 1;
    }
    char** databuf = new char* [nbufs];
    for (int i = 0; i < nbufs; i++) {
        databuf[i] = new char[bufsize];
    }

    switch ((int)argv[5]) {
    case 1:
        for (int i = 0; i < iterations; i++)
            for (int j = 0; j < nbufs; j++)
                write(sd, databuf[j], bufsize); // sd: socket descriptor
        break;
    case 2:
        for (int i = 0; i < iterations; i++) {
            iovec vector[nbufs];
            for (int j = 0; j < nbufs; j++) {
                vector[j].iov_base = databuf[j];
                vector[j].iov_len = bufsize;
            }
            writev(sd, vector, nbufs); // sd: socket descriptor
        }
        break;
    case 3:
        for (int i = 0; i < iterations; i++) {
            write(sd, databuf, nbufs * bufsize); // sd: socket descriptor
        }
        break;
    default:
        cout << "Bad type selection" << endl;
    }

    for (int i = 0; i < nbufs; i++) {
        delete[] databuf[i];
    }
    delete[] databuf;
}

/*void write(int sd, char* dataRow, int bufsize) {

}

void write(int sd, char** databuf, int bufsize) {

}

void writev(int sd, iovec vector, int nbufs) {

}*/

