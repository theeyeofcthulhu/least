#!/usr/bin/env bash

EXE=lcc
BUILD_COMMAND=ninja
CMAKE_OPTS=-GNinja
COMPILE_COMMANDS=true

set -xe

[[ "$1" == "clean" ]] && rm -rf ./build

mkdir -p build
cd build

cmake $CMAKE_OPTS ..
$BUILD_COMMAND

cd ..
[[ ! -f "./$EXE" ]] && ln -sv "build/$EXE" .
[[ ! -f "lib/libstdleast.a" ]] && ln -sv "build/libstdleast.a" .
[[ "$COMPILE_COMMANDS" = true ]] && [[ ! -f ./compile_commands.json ]] && ln -sv build/compile_commands.json .

[[ "$1" == "r" ]] && "./$EXE"
