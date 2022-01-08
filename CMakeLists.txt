cmake_minimum_required(VERSION 3.10)

project(lcc)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-g -Wall -Wextra -Wshadow -std=c++20 -pedantic")

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

file(GLOB SOURCES "${SRC_DIR}/*.cpp")

add_executable(lcc ${SOURCES})
