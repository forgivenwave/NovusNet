cmake_minimum_required(VERSION 3.10)
project(ls)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(ls
    src/server.cpp
    src/nn.cpp
)
add_executable(cl
    src/client.cpp
    src/nn.cpp
)

target_include_directories(ls PRIVATE include)
target_include_directories(cl PRIVATE include)
find_package(OpenSSL REQUIRED)
target_link_libraries(ls OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(cl OpenSSL::SSL OpenSSL::Crypto)