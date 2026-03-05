#include"nn.hpp"
#include<iostream>
#include <chrono>
int main(){
    runServer(9090);
    while(!clientConnected){
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    while(true){
        std::cout<<recvMsg()<<"\n";
    }
    return 0;
}