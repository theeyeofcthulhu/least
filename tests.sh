#!/usr/bin/env bash

FAIL=false

SHELL_GREEN="\033[0;32m"
SHELL_RED="\033[0;31m"
SHELL_WHITE="\033[0;37m"

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

FILES=(tests/hello_world.least tests/exit_code.least tests/if.least tests/ops.least tests/int.least)

for file in "${FILES[@]}" ; do
    echo "Compiling $file"
    test_program_compile "./lcc $file"
done

echo ""

FAIL=0

if [[ $(tests/hello_world) = "Hello, World!" ]]; then
    echo "tests/hello_world wrote \"Hello, World!\""
else
    echo "tests/hello_world did not write \"Hello, World!\""
    FAIL=1
fi


tests/exit_code
status=$?
if (( status == 1 )); then
    echo "tests/exit_code returned 1"
else
    echo "tests/exit_code did not return 1"
    FAIL=1
fi

if [[ $(tests/if) = $'2 is equal to 2\nSecond instruction inside if block\nnested if' ]]; then
    echo "tests/if printed expected results"
else
    echo "tests/if did not print expected results"
    FAIL=1
fi

if [[ $(tests/ops) = $'3 is less than 4\n4 is greater than 3\n4 is not 3' ]]; then
    echo "tests/ops printed expected results"
else
    echo "tests/ops did not print expected results"
    FAIL=1
fi

if [[ $(tests/int) = $'2 is less than 7\nsuccessfully used numeric constant and variable\nnested if with int\n9 is greater than 8\n1 equals 1' ]]; then
    echo "tests/int printed expected results"
else
    echo "tests/int did not print expected results"
    FAIL=1
fi

if (( $FAIL == 0 )); then
    echo -e "$SHELL_GREEN\nAll test succeeded$SHELL_WHITE"
else
    echo -e "$SHELL_RED\nSome tests failed$SHELL_WHITE"
    exit 1
fi
