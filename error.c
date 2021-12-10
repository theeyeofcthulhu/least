#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "error.h"

#define SHELL_RED   "\033[0;31m"
#define SHELL_WHITE "\033[0;37m"

char* error_file;

void compiler_error_core(int line, char* format, va_list format_list);

/* Functions for asserting something and exiting
 * if something is unwanted; compiler_error just
 * exits no matter what */

void compiler_error_on_false(bool eval, int line, char* format, ...){
    if(eval)
        return;

    va_list format_params;

    va_start(format_params, format);
    compiler_error_core(line, format, format_params);
    va_end(format_params);

    exit(1);
}

void compiler_error_on_true(bool eval, int line, char* format, ...){
    if(!eval)
        return;

    va_list format_params;

    va_start(format_params, format);
    compiler_error_core(line, format, format_params);
    va_end(format_params);

    exit(1);
}

void compiler_error(int line, char* format, ...){
    va_list format_params;

    va_start(format_params, format);
    compiler_error_core(line, format, format_params);
    va_end(format_params);

    exit(1);
}

void compiler_error_core(int line, char* format, va_list format_list){
    fprintf(stderr, "%sCompiler Error!\n", SHELL_RED);
    if(line == 0)
        fprintf(stderr, "%s: ", error_file ? error_file : "Initialization");
    else
        fprintf(stderr, "%s:%d ", error_file ? error_file : "Initialization", line);

    vfprintf(stderr, format, format_list);

    fprintf(stderr, SHELL_WHITE);
}

void set_error_file(char* source_file){
    compiler_error_on_false(source_file, 0, "Passing empty string as error file\n");
    error_file = source_file;
}
