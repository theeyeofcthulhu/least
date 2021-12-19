#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <string>

#include "error.h"

#define SHELL_RED "\033[0;31m"
#define SHELL_WHITE "\033[0;37m"

/* Functions for asserting something and exiting
 * if something is unwanted; compiler_error just
 * exits no matter what */

void error_handler::on_false(bool eval, std::string format, ...) {
    if (eval)
        return;

    va_list format_params;

    va_start(format_params, format);
    error_core(format, format_params);
    va_end(format_params);

    std::exit(1);
}

void error_handler::on_true(bool eval, std::string format, ...) {
    if (!eval)
        return;

    va_list format_params;

    va_start(format_params, format);
    error_core(format, format_params);
    va_end(format_params);

    std::exit(1);
}

void error_handler::error(std::string format, ...) {
    va_list format_params;

    va_start(format_params, format);
    error_core(format, format_params);
    va_end(format_params);

    std::exit(1);
}

void error_handler::error_core(std::string format, va_list format_list) {
    std::cerr << SHELL_RED << "Compiler Error!\n";

    if (m_line == -1)
        std::cerr << m_file << ": ";
    else
        std::cerr << m_file << ":" << m_line + 1 << " ";

    std::vfprintf(stderr, format.c_str(), format_list);

    std::cerr << SHELL_WHITE;
}
