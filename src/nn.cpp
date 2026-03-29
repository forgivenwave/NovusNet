#include "nn.hpp"
#include <string>
#include <map>
#include <functional>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <netinet/tcp.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <fcntl.h>
#include <cerrno>
#include <fstream>
#include <endian.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <mutex>
#include <atomic>
#include<memory>


std::map<int, SSL*> clients;
std::atomic<int> clients_index = 0;
std::function<void(int, std::string)> messageCallback;
SSL_CTX* ssl_ctx = nullptr;
SSL* client_ssl = nullptr;
std::mutex clients_mutex;

std::map<int, std::shared_ptr<std::mutex>> client_io_mutexes;
std::mutex client_ssl_io_mutex;

std::shared_ptr<std::mutex> getIOMutex(int id) {
    if (id == 0) return nullptr;
    std::lock_guard<std::mutex> lock(clients_mutex);
    auto it = client_io_mutexes.find(id);
    if (it != client_io_mutexes.end()) return it->second;
    auto m = std::make_shared<std::mutex>();
    client_io_mutexes[id] = m;
    return m;
}

void onMessage(std::function<void(int, std::string)> callback){
    messageCallback = callback;
}

SSL* getSSL(int id) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    auto it = clients.find(id);
    if (it != clients.end()) return it->second;
    return client_ssl;
}

void runServer(int port, std::string password) {
    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    if (!SSL_CTX_use_certificate_file(ctx, "cert.pem", SSL_FILETYPE_PEM)) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        return;
    }
    if (!SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM)) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        return;
    }
    ssl_ctx = ctx;
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0){ perror("socket failed"); return; }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    int b = bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    if(b < 0){ perror("bind failed"); return; }
    int l = listen(server_fd, 32);
    if(l < 0){ perror("listen failed"); return; }
    std::cout << "Server on port " << port << "\n";

    std::thread([server_fd,password]() {
        while (true) {
            sockaddr_in client_addr{};
            socklen_t len = sizeof(client_addr);
            int new_fd = accept(server_fd, (sockaddr*)&client_addr, &len);
            if(new_fd < 0) continue;
            SSL* ssl = SSL_new(ssl_ctx);
            SSL_set_fd(ssl, new_fd);
            if (SSL_accept(ssl) <= 0) {
                ERR_print_errors_fp(stderr);
                SSL_free(ssl);
                close(new_fd);
                continue;
            }
            int ci = ++clients_index;
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                clients[ci] = ssl;
                client_io_mutexes[ci] = std::make_shared<std::mutex>();
            }
            std::string recvdp = recvMsg(ci);
            if(recvdp!=password){
                std::cout << "KICKED (wrong password): " << inet_ntoa(client_addr.sin_addr) << "\n";
                {
                    std::lock_guard<std::mutex> lock(clients_mutex);
                    SSL_shutdown(clients[ci]);
                    SSL_free(clients[ci]);
                    clients.erase(ci);
                    client_io_mutexes.erase(ci);
                }
                close(new_fd);
            }else{
                std::cout << "CONNECTED: " << inet_ntoa(client_addr.sin_addr) << "\n";
                sendMsg(std::to_string(ci),ci);
                int new_ci = ci;
                SSL* new_ssl = ssl;
                std::thread([new_ci,new_ssl,new_fd]() {
                    while (true) {
                        std::string msg = recvMsg(new_ci);
                        if (msg=="EXITED(C-178)"){
                            std::cout<<"client "<<new_ci<<" disconnected, cleaning.\n";
                            {
                                std::lock_guard<std::mutex> lock(clients_mutex);
                                SSL_shutdown(clients[new_ci]);
                                SSL_free(clients[new_ci]);
                                clients.erase(new_ci);
                                client_io_mutexes.erase(new_ci);
                            }
                            close(new_fd);
                            break;
                        }
                        if (messageCallback) messageCallback(new_ci, msg);
                    }
                }).detach();
            }
        }
    }).detach();
}

int runClient(std::string ip, int port,std::string password) {
    int client = socket(AF_INET, SOCK_STREAM, 0);
    if(client<0){
        perror("socket failed");
        return -1;
    }
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serverAddress.sin_addr);

    if(connect(client, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1){
        perror("connect failed");
        close(client);
        return -1;
    }
    SSL_CTX* client_ctx = SSL_CTX_new(TLS_client_method());
    SSL_CTX_set_verify(client_ctx, SSL_VERIFY_PEER, nullptr);
    SSL_CTX_load_verify_locations(client_ctx, "cert.pem", nullptr);
    SSL* ssl = SSL_new(client_ctx);
    SSL_CTX_free(client_ctx);
    SSL_set_fd(ssl, client);
    if (SSL_connect(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(client);
        return -1;
    }
    if(SSL_get_verify_result(ssl) != X509_V_OK) {
        SSL_free(ssl);
        close(client);
        return -1;
    }
    client_ssl = ssl;
    sendMsg(password,0);
    std::cout << "Request sent\n";
    return client;
}

bool sendMsg(std::string msg, int id) {
    SSL* ssl;
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        ssl = (clients.count(id)) ? clients[id] : client_ssl;
    }
    int msglength = msg.size();
    uint32_t msglengthC = htonl(msglength);
    int bytesL = msglength;
    int bytesS = 0;
    int result=0;

    int headerL = sizeof(msglengthC);
    int headerS = 0;
    while(headerL > 0){
        result = SSL_write(ssl, (char*)&msglengthC + headerS, headerL);
        if(result <= 0){
            perror("send failed");
            return false;
        }
        headerS += result;
        headerL -= result;
    }
    while (bytesL > 0) {
        result = SSL_write(ssl, msg.c_str() + bytesS, bytesL);
        if (result <= 0) {
            perror("send failed");
            return false;
        }
        bytesS += result;
        bytesL -= result;
    }
    return true;
}

std::string recvMsg(int id) {
    SSL* ssl;
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        ssl = (clients.count(id)) ? clients[id] : client_ssl;
    }
    uint32_t msgL_htonl;
    int result=0;

    int headerL = sizeof(msgL_htonl);
    int headerR = 0;
    while(headerL > 0){
        result = SSL_read(ssl, (char*)&msgL_htonl + headerR, headerL);
        if(result <= 0){
            perror("recv failed");
            return "EXITED(C-178)";
        }
        headerR += result;
        headerL -= result;
    }
    int msgL = ntohl(msgL_htonl);
    if(msgL>4*1024*1024 || msgL<=0) return "EXITED(C-178)";
    int bytesR = 0;
    int bytesL = msgL;
    std::string msg(msgL, 0);

    while (bytesL > 0) {
        result = SSL_read(ssl, msg.data() + bytesR, bytesL);
        if (result <= 0) {
            perror("recv failed");
            return "EXITED(C-178)";
        }
        bytesR += result;
        bytesL -= result;
    }
    return msg;
}

auto printProgress = [](uint64_t done, uint64_t total) {
    int percent = (done * 100) / total;
    int filled = percent / 5;
    std::cout << "\r[";
    for (int i = 0; i < 20; i++)
        std::cout << (i < filled ? '#' : '-');
    std::cout << "] " << percent << "% " << std::flush;
};

bool sendFile(std::string filepath, int id) {
    std::shared_ptr<std::mutex> io_mtx;
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        auto it = client_io_mutexes.find(id);
        if (it != client_io_mutexes.end()) io_mtx = it->second;
    }
    std::unique_lock<std::mutex> io_lock(id == 0 ? client_ssl_io_mutex : *io_mtx);

    SSL* ssl = getSSL(id);
    if (!ssl) return false;

    int fd = open(filepath.c_str(), O_RDONLY);
    if (fd < 0) return false;

    struct stat st;
    fstat(fd, &st);
    uint64_t size = st.st_size;
    uint64_t netsize = htobe64(size);

    if (SSL_write(ssl, &netsize, sizeof(netsize)) <= 0) {
        close(fd);
        return false;
    }

    std::string filename = filepath.substr(filepath.find_last_of("/\\") + 1);
    if (!sendMsg(filename, id)) {
        close(fd);
        return false;
    }

    char buffer[16384];
    uint64_t totalSent = 0;
    while (totalSent < size) {
        ssize_t bytesRead = read(fd, buffer, sizeof(buffer));
        if (bytesRead <= 0) break;

        int offset = 0;
        while (offset < bytesRead) {
            int sent = SSL_write(ssl, buffer + offset, bytesRead - offset);
            if (sent <= 0) {
                close(fd);
                return false;
            }
            offset += sent;
            totalSent += sent;
        }
        printProgress(totalSent, size);
    }
    close(fd);
    return true;
}

bool recvFile(std::string folderpath, int id){
    std::shared_ptr<std::mutex> io_mtx;
    SSL* ssl;
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        ssl = (clients.count(id)) ? clients[id] : client_ssl;
        auto it = client_io_mutexes.find(id);
        if (it != client_io_mutexes.end()) io_mtx = it->second;
    }
    std::unique_lock<std::mutex> io_lock(id == 0 ? client_ssl_io_mutex : *io_mtx);

    char buffer[16384];
    uint64_t netsize;
    if(SSL_read(ssl,&netsize,sizeof(netsize))<=0){
        perror("file recv failed");
        return false;
    }
    uint64_t filesize = be64toh(netsize);
    if(filesize>10ULL*1024*1024*1024 || filesize<=0) return false;

    std::string filename = recvMsg(id);
    if(filename.empty() || filename.size() > 255) return false;
    if(filename.find('/') != std::string::npos) return false;

    uint64_t bytesL = filesize;
    uint64_t bytesR = 0;
    int result;
    std::string fullpath = folderpath + "/" + filename;
    int outfd = open(fullpath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(outfd < 0) return false;

    while(bytesL>0){
        result = SSL_read(ssl, buffer, std::min(bytesL, (uint64_t)sizeof(buffer)));
        if(result<=0){
            close(outfd);
            return false;
        }
        bytesL -= result;
        bytesR += result;
        ssize_t written = write(outfd, buffer, result);
        if(written != result){
            perror("write failed");
            close(outfd);
            return false;
        }
        printProgress(bytesR,filesize);
    }
    close(outfd);
    return true;
}