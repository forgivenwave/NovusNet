#pragma once
#include<string>
extern int client_fd;
void runServer(int port);
void sendMsg(std::string msg);