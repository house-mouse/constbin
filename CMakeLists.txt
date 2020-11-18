cmake_minimum_required(VERSION 3.11)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS "-Wall -Wextra")

project(constbin)

add_executable(constbin
   constbin.cpp
)


