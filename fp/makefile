all: tftp_client tftp_server

tftp_client: ./client/tftp_client.cpp
	g++ ./client/tftp_client.cpp -o client/tftp_client

tftp_server: ./server/tftp_server.cpp ./server/tftp.h ./server/tftp.cpp
	g++ -lpthread ./server/tftp_server.cpp ./server/tftp.h ./server/tftp.cpp -o server/tftp_server



