#!/usr/bin/env bash

EXE=lcc
BUILD_COMMAND=ninja
CMAKE_OPTS=-GNinja
COMPILE_COMMANDS=true

set -xe
shopt -s extglob

[[ ! -d "fmt-9.1.0" ]] && wget -q --output-document "fmt-9.1.0.zip" "https://github.com/fmtlib/fmt/releases/download/9.1.0/fmt-9.1.0.zip" && unzip -q "fmt-9.1.0.zip" && rm "fmt-9.1.0.zip"

[[ "$1" == "clean" ]] && rm -rf ./build && rm -f tests/!(*.txt|*.least)

mkdir -p build
cd build

cmake $CMAKE_OPTS ..
$BUILD_COMMAND

cd ..
ln -sfv "build/$EXE" .
ln -sfv "../build/libstdleast.a" "lib/libstdleast.a"
[[ "$COMPILE_COMMANDS" = true ]] && ln -sfv build/compile_commands.json .

[[ "$1" == "r" ]] && "./$EXE"

exit 0
