cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-g -Wall -Wextra -Wshadow -std=c++20 -pedantic")

project(lcc)

file(GLOB SOURCES "*.cpp")

add_executable(lcc ${SOURCES})
