#!/bin/sh

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
SHELL_WHITE="\033[0;0m"

if [[ $1 = "reload" ]]; then
    make

    echo -e "\nCompiling tests\n"

    for file in "${FILES[@]}" ; do
        echo "Compiling $file"
        test_program_compile "./lcc $file"
        executable=$(echo "$file" | sed 's/\..*//')
        txt="${executable}_results.txt"
        echo -e "Generating results to ${txt}\n"

        if [ "$executable" == "tests/yourname" ]; then
            $executable <<< 'tests.sh' > "${txt}"
        else
            $executable > "${txt}"
        fi
    done

    exit
elif [[ $1 = "valgrind" ]]; then
    VALGRIND="valgrind"
fi

FAIL=false

./build.sh

echo -e "\nCompiling tests\n"

for file in "${FILES[@]}" ; do
    echo "Compiling $file"
    test_program_compile "$VALGRIND ./lcc $file"
done

echo ""

FAIL=0

for file in "${FILES[@]}"; do
    executable=$(echo "$file" | sed 's/\..*//')
    txt="${executable}_results.txt"
    expected_output=$(cat $txt)

    if [ "$executable" == "tests/yourname" ]; then
        if [ "`$executable <<< 'tests.sh'`" != "${expected_output}" ]; then
            echo -e "${SHELL_RED}Test ${executable} failed${SHELL_WHITE}"
            FAIL=1
        else
            echo "Test ${executable} succeeded"
        fi
    elif [ "$executable" == "tests/time" ]; then
        echo -e "Time returned:\n$($executable)"
    else
        if [ "`$executable`" != "${expected_output}" ]; then
            echo -e "${SHELL_RED}Test ${executable} failed${SHELL_WHITE}"
            FAIL=1
        else
            echo "Test ${executable} succeeded"
        fi
    fi
done

if (( ${FAIL} == 1 )); then
    echo -e "\n${SHELL_RED}Some or all tests failed ${SHELL_WHITE}"
    exit 1
else
    echo -e "\n${SHELL_GREEN}All tests succeeded ${SHELL_WHITE}"
    exit 0
fi
