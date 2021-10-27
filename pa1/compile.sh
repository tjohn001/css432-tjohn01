#!/bin/sh

g++ -lpthread server.cpp -o server
g++ client.cpp -o client
chmod 744 server client