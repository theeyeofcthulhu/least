cmake_minimum_required(VERSION 3.22)

set(CMAKE_EXPORT_COMPILE_COMMANDS on)

project(lcc CXX ASM_NASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-g -Wall -Wextra -Wshadow -pedantic -Wno-deprecated-declarations")

set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB SOURCES "${SRC_DIR}/*.cpp")
file(GLOB ASM "${LIB_DIR}/*.asm")

add_subdirectory(./fmt-9.1.0 fmt-bin)
add_library(FMT STATIC IMPORTED)
set_target_properties(FMT PROPERTIES IMPORTED_LOCATION fmt-bin/libfmt.a)

add_executable(lcc ${SOURCES})
target_include_directories(lcc PRIVATE fmt-9.1.0/include)
target_link_libraries(lcc PRIVATE FMT)

add_library(stdleast ${ASM})
