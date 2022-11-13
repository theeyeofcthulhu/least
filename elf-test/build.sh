#!/bin/sh

g++ -std=gnu++20 -Wall -Wextra -g -o elf elf.cpp instruction.cpp -lfmt
./elf
