#!/usr/bin/env bash

set -e

if [[ $1 == "clean" ]]; then
    rm -rf ./build
fi

mkdir -p build
cd build

cmake -GNinja ..
ninja

cd ..
cp -v build/lcc .
cp -v build/compile_commands.json .
cp -v build/libstdleast.a lib/
