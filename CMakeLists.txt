cmake_minimum_required(VERSION 3.10)
project(NovusNet)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_executable(s
    src/main.cpp
    src/nn.cpp
)
target_include_directories(s PRIVATE include)
find_package(OpenSSL REQUIRED)
target_link_libraries(s OpenSSL::SSL OpenSSL::Crypto)