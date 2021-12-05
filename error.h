#ifndef ERROR_H_
#define ERROR_H_

#include <stdarg.h>
#include <stdbool.h>

void compiler_error_on_false(bool eval, int line, char* format, ...);
void compiler_error_on_true(bool eval, int line, char* format, ...);
void compiler_error(int line, char* format, ...);
void set_error_file(char* source_file);

#endif // ERROR_H_
