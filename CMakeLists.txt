cmake_minimum_required(VERSION 3.3)
project(8005_a2_scalableservers)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(E_SOURCE_FILES src/epoll/main.cpp src/epoll/ConnectionProcess.h src/epoll/ConnectionProcess.cpp)
set(S_SOURCE_FILES src/select/main.cpp src/select/ConnectionProcess.cpp src/select/ConnectionProcess.h)
set(T_SOURCE_FILES src/traditional/main.cpp src/traditional/ConnectionProcess.cpp src/traditional/ConnectionProcess.h)
set(C_SOURCE_FILES src/client/main.cpp)

add_executable(epoll ${E_SOURCE_FILES})
add_executable(traditional ${T_SOURCE_FILES})
add_executable(select ${S_SOURCE_FILES})
add_executable(client ${C_SOURCE_FILES})