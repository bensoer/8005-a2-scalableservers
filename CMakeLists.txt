cmake_minimum_required(VERSION 3.3)
project(8005_a2_scalableservers)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

#set(SOURCE_FILES main.cpp)
#add_executable(8005_a2_scalableservers ${SOURCE_FILES})

set(E_SOURCE_FILES src/epoll/main.cpp)
set(S_SOURCE_FILES src/select/main.cpp)
set(T_SOURCE_FILES src/traditional/main.cpp)

add_executable(epoll ${E_SOURCE_FILES})
add_executable(tradition ${T_SOURCE_FILES})
add_executable(select ${S_SOURCE_FILES})