#ifndef ERROR_H_
#define ERROR_H_

#include <iostream>
#include <string>

class error_handler {
  public:
    void on_false(bool eval, std::string format, ...);
    void on_true(bool eval, std::string format, ...);
    void error(std::string format, ...);
    void set_file(std::string file) { m_file = file; };
    void set_line(int line) { m_line = line; };

    error_handler(std::string file) : m_file(file) {}

  private:
    std::string m_file;
    int m_line = -1;
    void error_core(std::string format, va_list format_list);
};

#endif // ERROR_H_
