#pragma once
#include<string>
extern int client_fd;
//Initialization
void initClient(std::string ip, int port);

//Connecting
void runServer(int port);
void runClient(const char* ip, int port);

//after connection
void sendMsg(std::string msg);
std::string recvMsg();