cmake_minimum_required(VERSION 3.10)

project(lcc CXX ASM_NASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-g -Wall -Wextra -Wshadow -std=c++20 -pedantic")

set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib)

file(GLOB SOURCES "${SRC_DIR}/*.cpp")
file(GLOB ASM "${LIB_DIR}/*.asm")

add_executable(lcc ${SOURCES})

add_library(stdleast ${ASM})
