#include"conn.hpp"
#include<string>
#include<sys/socket.h>
#include <netinet/in.h> // Internet address structures
#include <arpa/inet.h> // IP address conversion
#include <unistd.h>
#include<cstring>
#include<iostream>
#include <netinet/tcp.h>
using namespace std;
int client_fd;
void runServer(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    bind(server_fd,(sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 32);

    std::cout << "Server listening on port " << port << "\n";

    while (true) {
        sockaddr_in client_addr{};
        socklen_t len = sizeof(client_addr);
        client_fd = accept(server_fd, (sockaddr*)&client_addr, &len);
        if (client_fd < 0) continue;
        std::cout << "CONNECTED:  " << inet_ntoa(client_addr.sin_addr) << "\n";
        sendMsg("GO FUCK YOURSELF BROTHER!!!");
    }
}
void sendMsg(std::string msg){
    send(client_fd, msg.c_str(), msg.size(), 0);
}