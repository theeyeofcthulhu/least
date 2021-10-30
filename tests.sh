#!/usr/bin/env bash

function test_program_compile {
    $@ > /dev/null
    local status=$?
    if (( status != 0 )); then
        echo "error with $1"
    fi
    return $status
}

FILES=($(ls tests/*.least))
echo -e "Testing these files: ${FILES[@]}\n"

SHELL_GREEN="\033[0;32m"
SHELL_RED="\033[0;31m"
SHELL_WHITE="\033[0;37m"

if [[ $1 = "reload" ]]; then
    make

    echo -e "\nCompiling tests\n"

    for file in "${FILES[@]}" ; do
        echo "Compiling $file"
        test_program_compile "./lcc $file"
        executable=$(echo "$file" | sed 's/\..*//')
        txt="${executable}_results.txt"
        echo -e "Generating results to ${txt}\n"
        $executable > "${txt}"
    done

    exit
fi

FAIL=false

make

echo -e "\nCompiling tests\n"

for file in "${FILES[@]}" ; do
    echo "Compiling $file"
    test_program_compile "./lcc $file"
done

echo ""

FAIL=0

for file in "${FILES[@]}"; do
    executable=$(echo "$file" | sed 's/\..*//')
    txt="${executable}_results.txt"
    expected_output=$(cat $txt)

    # echo $expected_output
    # echo "`$executable`"

    if [ "`$executable`" != "${expected_output}" ]; then
        echo -e "${SHELL_RED}Test ${executable} failed${SHELL_WHITE}"
        FAIL=1
    else
        echo "Test ${executable} succeeded"
    fi
done

if (( ${FAIL} == 1 )); then
    echo -e "\n${SHELL_RED}Some or all tests failed ${SHELL_WHITE}"
else
    echo -e "\n${SHELL_GREEN}All tests succeeded ${SHELL_WHITE}"
fi
