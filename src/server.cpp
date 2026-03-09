#include "nn.hpp"
#include <iostream>
#include <chrono>

int main(){
    runServer(9090);
    //"onMessage" returns clientFD and msg of any received message from any client.
    onMessage([](int clientFD, std::string msg){
        //more detailed logic can go on here depending on what you wanna do.
        std::cout << "Client " << clientFD << ": " << msg << "\n";
    });
    while(true){
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}