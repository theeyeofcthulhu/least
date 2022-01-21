#ifndef ERROR_H_
#define ERROR_H_

#include <iostream>
#include <string>

class ErrorHandler {
public:
    template<typename... args>
    void on_false(bool eval, const std::string& format, const args&... fargs);

    template<typename... args>
    void on_true(bool eval, const std::string& format, const args&... fargs);

    template<typename... args>
    void error(const std::string& format, const args&... fargs);

    template<typename... args>
    static void dbgln(const std::string& format, const args&... fargs);

    void set_file(const std::string& file) { m_file = file; };
    void set_line(int line) { m_line = line; };

    ErrorHandler(const std::string& file)
        : m_file(file)
    {
    }

private:
    std::string m_file;
    int m_line = -1;

    template<typename... args>
    void print_error(const std::string& format, const args&... fargs);

    template<typename It>
    static void error_core(std::ostream& out, It first, It last);

    template<typename It, typename T, typename... args>
    static void error_core(std::ostream& out,
        It first,
        It last,
        const T& arg,
        const args&... fargs);
};

template<typename... args>
void ErrorHandler::on_false(bool eval, const std::string& format, const args&... fargs)
{
    if (eval)
        return;

    print_error(format, fargs...);

    std::exit(1);
}

template<typename... args>
void ErrorHandler::on_true(bool eval, const std::string& format, const args&... fargs)
{
    if (!eval)
        return;

    print_error(format, fargs...);

    std::exit(1);
}

template<typename... args>
void ErrorHandler::error(const std::string& format, const args&... fargs)
{
    print_error(format, fargs...);

    std::exit(1);
}

template<typename... args>
void ErrorHandler::print_error(const std::string& format, const args&... fargs)
{
    std::cerr << "Compiler error!\n"
              << m_file << ":" << m_line << ": ";
    error_core(std::cerr, format.begin(), format.end(), fargs...);
}

template<typename... args>
void ErrorHandler::dbgln(const std::string& format, const args&... fargs)
{
    error_core(std::cout, format.begin(), format.end(), fargs...);
    std::cout << '\n';
}

template<typename It>
void ErrorHandler::error_core(std::ostream& out, It first, It last)
{
    for (auto it = first; it != last; it++) {
        switch (*it) {
        case '%':
            if (*(it + 1) == '%') {
                out << '%';
                it++;
                break;
            } else {
                out << "Too few arguments to error function\n";
                exit(1);
            }
        default:
            out << *it;
            break;
        }
    }
}

template<typename It, typename T, typename... args>
void ErrorHandler::error_core(std::ostream& out,
    It first,
    It last,
    const T& arg,
    const args&... fargs)
{
    for (auto it = first; it != last; it++) {
        switch (*it) {
        case '%':
            if (*(it + 1) == '%') {
                out << '%';
                it++;
                break;
            } else {
                out << arg;
                return error_core(out, ++it, last, fargs...);
            }
        default:
            std::cerr << *it;
            break;
        }
    }
}

#endif // ERROR_H_
