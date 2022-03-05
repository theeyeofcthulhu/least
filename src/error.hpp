#ifndef ERROR_H_
#define ERROR_H_

#include <fmt/color.h>
#include <fmt/ostream.h>
#include <iostream>
#include <string>

#include "macros.hpp"

class ErrorHandler {
public:
    template<typename T, typename... args>
    void on_false(bool eval, T&& format, args&&... fargs);

    template<typename T, typename... args>
    void on_true(bool eval, T&& format, args&&... fargs);

    template<typename T, typename... args>
    void error(T&& format, args&&... fargs);

    void set_file(std::string_view file) { m_file = file; };
    void set_line(int line) { m_line = line; };

    ErrorHandler(std::string_view file)
        : m_file(file)
    {
    }

private:
    std::string_view m_file;
    int m_line = -1;

    template<typename T, typename... args>
    void print_error(T&& format, args&&... fargs);
};

template<typename T, typename... args>
void ErrorHandler::on_false(bool eval, T&& format, args&&... fargs)
{
    if (eval)
        return;

    print_error(std::forward<T>(format), std::forward<args>(fargs)...);

    std::exit(1);
}

template<typename T, typename... args>
void ErrorHandler::on_true(bool eval, T&& format, args&&... fargs)
{
    if (!eval)
        return;

    print_error(std::forward<T>(format), std::forward<args>(fargs)...);

    std::exit(1);
}

template<typename T, typename... args>
void ErrorHandler::error(T&& format, args&&... fargs)
{
    print_error(std::forward<T>(format), std::forward<args>(fargs)...);

    std::exit(1);
}

template<typename T, typename... args>
void ErrorHandler::print_error(T&& format, args&&... fargs)
{
    fmt::print(std::cerr, "{}\n{}:{}: {}\n", RED_ARG("Compiler error!"), m_file, m_line + 1,
        fmt::vformat(std::forward<T>(format), fmt::make_format_args(std::forward<args>(fargs)...)));
}

#endif // ERROR_H_
