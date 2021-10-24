#!/usr/bin/env bash

function test_program_compile {
    $@ > /dev/null
    local status=$?
    if (( status != 0 )); then
        echo "error with $1"
    fi
    return $status
}

make

echo -e "\nCompiling tests\n"

FILES=(tests/hello_world.least tests/exit_code.least)

for file in "${FILES[@]}" ; do
    echo "Compiling $file"
    test_program_compile "./lcc $file"
done

echo ""

if [[ $(tests/hello_world) = "Hello, World!" ]]; then
    echo "tests/hello_world wrote \"Hello, World!\""
else
    echo "tests/hello_world did not write \"Hello, World!\""
fi


tests/exit_code
status=$?
if (( status == 1 )); then
    echo "tests/exit_code returned 1"
else
    echo "tests/exit_code did not return 1"
fi
