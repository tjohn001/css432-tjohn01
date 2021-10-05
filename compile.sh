#!/bin/sh

gcc echo_server.c -o echo_server
gcc echo_client.c -o echo_client
chmod 744 echo_server echo_client
