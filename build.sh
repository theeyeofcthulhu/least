#!/usr/bin/env bash

mkdir -p build
cd build

cmake -GNinja ..
ninja

cd ..
cp build/lcc .
