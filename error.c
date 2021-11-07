#include "error.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#define SHELL_RED   "\033[0;31m"
#define SHELL_WHITE "\033[0;37m"

void compiler_error_core(char* source_file, int line, char* format, va_list format_list);

/* Functions for asserting something and exiting
 * if something is unwanted; compiler_error just
 * exits no matter what */

void compiler_error_on_false(bool eval, char* source_file, int line, char* format, ...){
    if(eval)
        return;

    va_list format_params;

    va_start(format_params, format);
    compiler_error_core(source_file, line, format, format_params);
    va_end(format_params);

    exit(1);
}

void compiler_error_on_true(bool eval, char* source_file, int line, char* format, ...){
    if(!eval)
        return;

    va_list format_params;

    va_start(format_params, format);
    compiler_error_core(source_file, line, format, format_params);
    va_end(format_params);

    exit(1);
}

void compiler_error(char* source_file, int line, char* format, ...){
    va_list format_params;

    va_start(format_params, format);
    compiler_error_core(source_file, line, format, format_params);
    va_end(format_params);

    exit(1);
}

void compiler_error_core(char* source_file, int line, char* format, va_list format_list){
    fprintf(stderr, "%sCompiler Error!\n", SHELL_RED);
    if(line == 0)
        fprintf(stderr, "%s: ", source_file);
    else
        fprintf(stderr, "%s:%d ", source_file, line);

    vfprintf(stderr, format, format_list);

    fprintf(stderr, SHELL_WHITE);
}
