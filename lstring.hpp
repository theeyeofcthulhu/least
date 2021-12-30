#ifndef LSTRING_H_
#define LSTRING_H_

#include <map>
#include <memory>
#include <string>

class compile_info;

namespace lexer {

class lstr;
class num;

std::shared_ptr<lstr> parse_string(std::string string, int line,
                                          compile_info &c_info);
std::shared_ptr<num> parse_char(std::string string, int line,
                                   compile_info &c_info);
} // namespace lexer

#endif // LSTRING_H_
