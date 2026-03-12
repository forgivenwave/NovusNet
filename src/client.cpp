#include "nn.hpp"
#include <iostream>

int main(){
    std::string msg;
    //runClient(ip,port) connects to a server
    int client = runClient("127.0.0.1", 9090,"PassTest");
    //receive client id and assign it for later use
    msg = recvMsg(client);
    int clientID = std::stoi(msg);
    while(true){
        std::getline(std::cin,msg);
        //sendMsg(string msg) sends data as a string
        sendMsg(msg,clientID);
    }
    return 0;
}