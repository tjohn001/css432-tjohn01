client.out: ./client/tftp_client.cpp
	g++ ./client/tftp_client.cpp -o client.out

server.out: ./server/tftp_server.cpp ./server/tftp.h ./server/tftp.cpp
	g++ -lpthread ./server/tftp_server.cpp ./server/tftp.h ./server/tftp.cpp -o server.out



