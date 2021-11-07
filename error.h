#ifndef ERROR_H_
#define ERROR_H_

#include <stdarg.h>
#include <stdbool.h>

void compiler_error_on_false(bool eval, char* source_file, int line, char* format, ...);
void compiler_error_on_true(bool eval, char* source_file, int line, char* format, ...);
void compiler_error(char* source_file, int line, char* format, ...);
void compiler_error_core(char* source_file, int line, char* format, va_list format_list);

#endif // ERROR_H_
